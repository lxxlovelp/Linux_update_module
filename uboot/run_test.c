
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int run_cmd(const char *cmd)
{
    int ret;

    printf("执行: %s\n", cmd);
    fflush(stdout);

    ret = system(cmd);

    if (ret != 0) {
        fprintf(stderr, "命令执行失败: %s\n", cmd);
        return -1;
    }

    return 0;
}

int main(void)
{
    /*
     * 设置 U-Boot 的 bootcmd：
     *
     * 1. 选择 SD 卡 mmc0
     * 2. 从 FAT32 分区加载 zImage
     * 3. 从 FAT32 分区加载 DTB
     * 4. 设置 bootargs
     * 5. 启动 Linux
     */
    const char *cmd =
        "fw_setenv bootcmd "
        "\"mmc dev 0; "
        "fatload mmc 0:1 0x80800000 zImage; "
        "fatload mmc 0:1 0x83000000 imx6ull-mmc-npi.dtb; "
        "setenv bootargs 'console=ttymxc0,115200 "
        "root=/dev/mmcblk0p3 "
        "rootfstype=ext4 rw'; "
        "bootz 0x80800000 - 0x83000000\"";

    printf("准备通过 fw_setenv 修改 U-Boot bootcmd...\n\n");

    /*
     * 第一步：
     * 写入 bootcmd
     */
    if (run_cmd(cmd) != 0)
        return 1;

    /*
     * 第二步：
     * 立即读取，确认写入成功
     */
    printf("\n写入后的 bootcmd:\n");

    if (run_cmd("fw_printenv bootcmd") != 0)
        return 1;

    /*
     * 第三步：
     * 确保数据同步到存储设备
     */
    printf("\n同步数据...\n");

    if (run_cmd("sync") != 0)
        return 1;

    /*
     * 第四步：
     * 给用户一点时间看到输出
     */
    printf("\nbootcmd 设置完成。\n");
    printf("2 秒后重启进入 U-Boot...\n");
    fflush(stdout);

    sleep(2);

    /*
     * 第五步：
     * 重启
     */
    if (run_cmd("reboot") != 0)
        return 1;

    return 0;
}

