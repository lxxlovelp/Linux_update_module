#include <fcntl.h>
#include <linux/watchdog.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define WATCHDOG_DEV "/dev/watchdog0"
#define WATCHDOG_TIMEOUT 30

static int watchdog_fd = -1;

/* 喂狗 */
static void feed_watchdog(void)
{
    char c = '\0';

    if (write(watchdog_fd, &c, 1) < 0) {
        perror("write watchdog");
    }
}

/* 获取 upgrade_available */
static int ota_pending(void)
{
    FILE *fp;
    char buf[32] = {0};

    fp = popen("fw_printenv -n upgrade_available 2>/dev/null", "r");
    if (fp == NULL)
        return 0;

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        pclose(fp);
        return 0;
    }

    pclose(fp);

    return (buf[0] == '1');
}

/* OTA启动成功确认 */
static void ota_boot_success(void)
{
    system("fw_setenv upgrade_available 0");
    system("fw_setenv bootcount 0");
    sync();

    printf("[WATCHDOG] OTA boot success!\n");
    fflush(stdout);
}

int main(void)
{
    int timeout = WATCHDOG_TIMEOUT;
    int i;

    printf("[WATCHDOG] Starting...\n");

    watchdog_fd = open(WATCHDOG_DEV, O_WRONLY);

    if (watchdog_fd < 0) {
        perror("[WATCHDOG] open watchdog");
        return 1;
    }

    /* 设置30秒超时 */
    if (ioctl(watchdog_fd, WDIOC_SETTIMEOUT, &timeout) < 0) {
        perror("[WATCHDOG] WDIOC_SETTIMEOUT");
        return 1;
    }

    printf("[WATCHDOG] timeout=%d seconds\n", timeout);
    fflush(stdout);

    /*
     * 如果不是OTA升级，
     * 直接正常喂狗。
     */
    if (!ota_pending()) {

        printf("[WATCHDOG] No OTA pending.\n");
        fflush(stdout);

        while (1) {
            feed_watchdog();
            sleep(10);
        }
    }

    /*
     * OTA正在等待确认。
     *
     * 这里故意不喂狗。
     *
     * 给Linux 30秒时间完成启动确认。
     */
    printf("[WATCHDOG] OTA pending!\n");
    printf("[WATCHDOG] Waiting for Linux boot success...\n");
    fflush(stdout);

    /*
     * 每秒检查一次 upgrade_available。
     *
     * 注意：
     * watchdog本身已经开始计时，
     * 所以不能超过30秒。
     */
    for (i = 0; i < 25; i++) {

        sleep(1);

        if (!ota_pending()) {

            printf("[WATCHDOG] Linux boot success detected!\n");
            fflush(stdout);

            /*
             * 已经被Linux成功确认，
             * 开始正常喂狗。
             */
            while (1) {
                feed_watchdog();
                sleep(10);
            }
        }
    }

    /*
     * 到这里仍然是 upgrade_available=1，
     * 说明Linux没有成功确认。
     *
     * 故意不喂狗。
     */
    printf("[WATCHDOG] Linux boot failed!\n");
    printf("[WATCHDOG] Stop feeding watchdog.\n");
    printf("[WATCHDOG] Waiting for hardware reset...\n");
    fflush(stdout);

    while (1)
        sleep(1);

    return 0;
}