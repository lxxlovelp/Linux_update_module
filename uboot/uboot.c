#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OTA_DIR        "/tmp/ota"
#define ROOTFS_IMAGE   OTA_DIR "/rootfs.img"

#define ROOTFS_A_PART  "/dev/mmcblk0p2"
#define ROOTFS_B_PART  "/dev/mmcblk0p3"


/* 执行 shell 命令 */
static int run_command(const char *cmd)
{
    int ret;

    printf("[OTA] 执行: %s\n", cmd);
    fflush(stdout);

    ret = system(cmd);

    if (ret != 0) {
        printf("[OTA] 命令执行失败!\n");
        return -1;
    }

    return 0;
}


/* 检查升级包 */
static int check_ota_files(void)
{
    if (access(ROOTFS_IMAGE, F_OK) != 0) {
        printf("[OTA] 找不到升级文件: %s\n", ROOTFS_IMAGE);
        return -1;
    }

    printf("[OTA] RootFS 升级包检查通过。\n");

    return 0;
}


/* 获取当前运行槽位 */
static char get_current_slot(void)
{
    FILE *fp;
    char buf[128] = {0};

    fp = popen("fw_printenv -n boot_slot 2>/dev/null", "r");

    if (fp == NULL) {
        return 'E';
    }
    // 读取 boot_slot 的值
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        pclose(fp);
        return 'E';
    }

    pclose(fp);

    if (buf[0] == 'A') {
        return 'A';
    }

    if (buf[0] == 'B') {
        return 'B';
    }

    return 'E';
}


/* 写入 RootFS */
static int write_rootfs(char target_slot)
{
    char cmd[256];
    const char *target_part;

    if (target_slot == 'A') {
        target_part = ROOTFS_A_PART;
    } else if (target_slot == 'B') {
        target_part = ROOTFS_B_PART;
    } else {
        printf("[OTA] 错误：非法目标 Slot\n");
        return -1;
    }

    snprintf(cmd,
             sizeof(cmd),
             "dd if=%s of=%s bs=4M conv=fsync status=none",
             ROOTFS_IMAGE,
             target_part);

    if (run_command(cmd) != 0) {
        return -1;
    }

    return 0;
}


/* 设置 U-Boot OTA 环境 */
static int set_uboot_env(char target_slot)
{
    char cmd[128];

    /* 设置当前启动 Slot */
    snprintf(cmd,
             sizeof(cmd),
             "fw_setenv boot_slot %c",
             target_slot);

    if (run_command(cmd) != 0) {
        return -1;
    }

    /* 标记存在待验证的新系统 */
    if (run_command("fw_setenv upgrade_available 1") != 0) {
        return -1;
    }

    /* 清零启动失败次数 */
    if (run_command("fw_setenv bootcount 0") != 0) {
        return -1;
    }

    return 0;
}


/* 主函数 */
int main(void)
{
    char current_slot;
    char target_slot;

    printf("==============================\n");
    printf("       OTA 升级程序\n");
    printf("==============================\n");

    /* 1. 检查升级包 */
    if (check_ota_files() != 0) {
        printf("[OTA] 升级包检查失败，终止升级。\n");
        return 1;
    }

    /* 2. 获取当前 Slot */
    current_slot = get_current_slot();

    if (current_slot == 'E') {
        printf("[OTA] 无法获取当前 Slot，终止升级。\n");
        return 1;
    }

    printf("[OTA] 当前运行 Slot: %c\n", current_slot);

    /* 3. 选择另一个 Slot */
    if (current_slot == 'A') {
        target_slot = 'B';
    } else {
        target_slot = 'A';
    }

    printf("[OTA] 目标 Slot: %c\n", target_slot);

    /* 4. 写入 RootFS */
    printf("[OTA] 开始写入 RootFS...\n");

    if (write_rootfs(target_slot) != 0) {
        printf("[OTA] RootFS 写入失败！\n");
        return 1;
    }

    printf("[OTA] RootFS 写入成功。\n");

    /* 5. 同步 */
    printf("[OTA] 同步文件系统...\n");

    if (run_command("sync") != 0) {
        printf("[OTA] sync 失败！\n");
        return 1;
    }

    /* 6. 修改 U-Boot 环境 */
    printf("[OTA] 更新 U-Boot 环境...\n");

    if (set_uboot_env(target_slot) != 0) {
        printf("[OTA] U-Boot 环境更新失败！\n");
        return 1;
    }

    /* 7. 显示最终环境 */
    printf("\n[OTA] 当前 U-Boot 环境:\n");

    run_command("fw_printenv boot_slot");
    run_command("fw_printenv upgrade_available");
    run_command("fw_printenv bootcount");

    /* 8. 重启 */
    printf("\n[OTA] OTA 升级准备完成。\n");
    printf("[OTA] 2 秒后重启系统...\n");

    sleep(2);

    run_command("sync");

    printf("[OTA] 正在重启...\n");

    run_command("reboot");

    return 0;
}