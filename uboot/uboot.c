#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 定义升级包文件路径 */
#define OTA_DIR             "/tmp/ota"
#define KERNEL_IMAGE        OTA_DIR "/kernel.img"
#define DTB_IMAGE           OTA_DIR "/dtb.img"
#define ROOTFS_IMAGE        OTA_DIR "/rootfs.img"

/* 定义 A/B 分区设备节点 */
#define KERNEL_A_PART       "/dev/mmcblk0p5"
#define KERNEL_B_PART       "/dev/mmcblk0p6"
#define DTB_A_PART          "/dev/mmcblk0p7"
#define DTB_B_PART          "/dev/mmcblk0p8"
#define ROOTFS_A_PART       "/dev/mmcblk0p9"
#define ROOTFS_B_PART       "/dev/mmcblk0p10"

/*
 * 基础函数：执行 shell 命令
 */
int run_command(const char *cmd)
{
    int ret;
    printf("[OTA] 执行命令: %s\n", cmd);
    
    ret = system(cmd);
    if (ret != 0) {
        printf("[OTA] 命令执行失败!\n");
        return -1;
    }
    return 0;
}

/*
 * 第一步：检查三个升级文件是否都存在
 * 使用 access 函数，不需要结构体
 */
int check_ota_files(void)
{
    if (access(KERNEL_IMAGE, F_OK) != 0) {
        printf("[OTA] 找不到文件: %s\n", KERNEL_IMAGE);
        return -1;
    }
    if (access(DTB_IMAGE, F_OK) != 0) {
        printf("[OTA] 找不到文件: %s\n", DTB_IMAGE);
        return -1;
    }
    if (access(ROOTFS_IMAGE, F_OK) != 0) {
        printf("[OTA] 找不到文件: %s\n", ROOTFS_IMAGE);
        return -1;
    }
    printf("[OTA] 升级文件检查通过。\n");
    return 0;
}

/*
 * 第二步：获取当前运行的槽位 (A 或 B)
 */
char get_current_slot(void)
{
    FILE *fp;
    char buf[128] = {0};

    /* 使用 -n 参数只打印值，去掉变量名，结果只会是 A 或 B */
    /*
    cfw_printenv：读取 U-Boot 的环境变量
    -n：只输出变量的值，不输出 boot_slot=
    boot_slot：要读取的变量
    2>/dev/null：把错误信息丢掉，不显示
    */
    fp = popen("fw_printenv -n boot_slot 2>/dev/null", "r");
    if (fp == NULL) {
        return 'E'; /* E 代表 Error 错误 */
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        pclose(fp);
        return 'E';
    }
    pclose(fp);

    /* 检查读取到的第一个字符 */
    if (buf[0] == 'A') {
        return 'A';
    }
    if (buf[0] == 'B') {
        return 'B';
    }

    return 'E'; /* 都不是，返回错误 */
}

/*
 * 第三步：写入 Kernel
 */
int write_kernel(char target_slot)
{
    char cmd[256];
    char target_part[64];

    if (target_slot == 'A') {
        strcpy(target_part, KERNEL_A_PART);
    } else {
        strcpy(target_part, KERNEL_B_PART);
    }

    sprintf(cmd, "dd if=%s of=%s bs=4M conv=fsync status=none", KERNEL_IMAGE, target_part);
    return run_command(cmd);
}

/*
 * 第四步：写入 DTB
 */
int write_dtb(char target_slot)
{
    char cmd[256];
    char target_part[64];

    if (target_slot == 'A') {
        strcpy(target_part, DTB_A_PART);
    } else {
        strcpy(target_part, DTB_B_PART);
    }

    sprintf(cmd, "dd if=%s of=%s bs=4M conv=fsync status=none", DTB_IMAGE, target_part);
    return run_command(cmd);
}

/*
 * 第五步：写入 RootFS
 */
int write_rootfs(char target_slot)
{
    char cmd[256];
    char target_part[64];

    if (target_slot == 'A') {
        strcpy(target_part, ROOTFS_A_PART);
    } else {
        strcpy(target_part, ROOTFS_B_PART);
    }

    sprintf(cmd, "dd if=%s of=%s bs=4M conv=fsync status=none", ROOTFS_IMAGE, target_part);
    return run_command(cmd);
}

/*
 * 第六步：设置 U-Boot 环境变量
 */
int set_uboot_env(char target_slot)
{
    char cmd[128];
    int ret;

    /* 1. 设置下次启动的分区 */
    sprintf(cmd, "fw_setenv boot_slot %c", target_slot);
    ret = run_command(cmd);
    if (ret != 0) return -1;

    /* 2. 告诉 U-Boot 有新系统需要验证 */
    sprintf(cmd, "fw_setenv upgrade_available 1");
    ret = run_command(cmd);
    if (ret != 0) return -1;

    /* 3. 清零失败计数器 (非常重要) */
    sprintf(cmd, "fw_setenv bootcount 0");
    ret = run_command(cmd);
    if (ret != 0) return -1;

    return 0;
}

/*
 * 主流程
 */
int main(void)
{
    char current_slot;
    char target_slot;

    printf("==============================\n");
    printf("       OTA 升级程序启动\n");
    printf("==============================\n");

    /* 1. 检查文件 */
    if (check_ota_files() != 0) {
        printf("[OTA] 错误：升级文件缺失，终止升级。\n");
        return -1;
    }

    /* 2. 获取当前 Slot */
    current_slot = get_current_slot();
    if (current_slot == 'E') {
        printf("[OTA] 错误：无法获取当前运行的分区，终止升级。\n");
        return -1;
    }
    printf("[OTA] 当前运行的分区是: %c\n", current_slot);

    /* 3. 决定目标 Slot */
    if (current_slot == 'A') {
        target_slot = 'B';
    } else {
        target_slot = 'A';
    }
    printf("[OTA] 准备将新系统写入: %c\n", target_slot);

    /* 4. 开始写入 */
    if (write_kernel(target_slot) != 0) {
        printf("[OTA] 写入 Kernel 失败！\n");
        return -1;
    }

    if (write_dtb(target_slot) != 0) {
        printf("[OTA] 写入 DTB 失败！\n");
        return -1;
    }

    if (write_rootfs(target_slot) != 0) {
        printf("[OTA] 写入 RootFS 失败！\n");
        return -1;
    }

    /* 5. 修改环境变量 */
    if (set_uboot_env(target_slot) != 0) {
        printf("[OTA] 设置 U-Boot 环境变量失败！\n");
        return -1;
    }

    /* 6. 重启系统 */
    printf("[OTA] 升级完成，系统正在保存数据...\n");
    run_command("sync");
    
    printf("[OTA] 正在重启...\n");
    run_command("reboot");

    return 0;
}