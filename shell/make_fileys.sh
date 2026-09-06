
#!/bin/bash

set -e
set -u


# 三个层次：从物理到逻辑
# 文件系统并不是直接操作硬盘的物理盘片，而是通过层层抽象，让你能“看见”文件和文件夹。

# 物理层：硬盘本身被划分成一个个固定大小的“格子”，称为数据块（Block）。这是硬盘读写的最小单位，就像仓库里统一规格的货架隔间。

# 逻辑层：这是文件系统的核心。它维护着一张巨大的“表格”（类似于FAT表或inode表），记录着哪个文件使用了哪些数据块、文件名叫什么、多大、创建时间等。文件在物理上可能东一块西一块（碎片），但在表格的“指引”下，逻辑上是连续的。

# 用户层：这就是你在电脑上打开的“我的电脑”或文件夹窗口。你看到的树状目录（根目录、子文件夹），是文件系统根据逻辑层的表格，为你呈现出的友好视图。
# ============================================================
# i.MX6ULL SD Card 镜像制作脚本
#
# 镜像布局：
#
#   LBA 0
#   ├── MBR 分区表
#   │
#   ├── 1KiB
#   │   └── U-Boot
#   │
#   ├── P1: 4MiB ~ 100MiB
#   │   └── FAT32
#   │       ├── zImage
#   │       └── imx6ull-mmc-npi.dtb
#   │
#   ├── P2: 100MiB ~ 600MiB
#   │   └── ext4
#   │       └── RootFS_A
#   │
#   ├── P3: 600MiB ~ 1100MiB
#   │   └── ext4
#   │       └── RootFS_B
#   │
#   └── 1100MiB ~ 1200MiB
#       └── 预留空间
#
# RootFS：
#   results/rootfs.cpio.gz
#
# ============================================================


# ============================================================
# 配置
# ============================================================

BASE_DIR="/home/xingxinliao/update/system_img"

IMAGE_FILE="${BASE_DIR}/factory_emmc_user.img"
IMAGE_SIZE_MB=1200

# ------------------------------------------------------------
# 输入文件
# ------------------------------------------------------------

UBOOT_IMX="${BASE_DIR}/results/u-boot-dtb.imx"
KERNEL_BIN="${BASE_DIR}/results/zImage"
DTB_BIN="${BASE_DIR}/results/imx6ull-mmc-npi.dtb"
ROOTFS_IMG="${BASE_DIR}/results/rootfs.cpio.gz"

DTB_FILENAME=$(basename "$DTB_BIN")


# ============================================================
# 变量
# ============================================================

LOOP_DEV=""
WORK_DIR=""

BOOT_MOUNT=""
ROOTFS_A_MOUNT=""
ROOTFS_B_MOUNT=""
ROOTFS_DIR=""


# ============================================================
# 清理函数
# ============================================================

cleanup()
{
    echo
    echo "=========================================="
    echo "执行清理"
    echo "=========================================="

    set +e

    if [ -n "${BOOT_MOUNT:-}" ]; then
        umount "$BOOT_MOUNT" 2>/dev/null
    fi

    if [ -n "${ROOTFS_A_MOUNT:-}" ]; then
        umount "$ROOTFS_A_MOUNT" 2>/dev/null
    fi

    if [ -n "${ROOTFS_B_MOUNT:-}" ]; then
        umount "$ROOTFS_B_MOUNT" 2>/dev/null
    fi

    if [ -n "${LOOP_DEV:-}" ]; then
        losetup -d "$LOOP_DEV" 2>/dev/null
    fi

    if [ -n "${WORK_DIR:-}" ] && [ -d "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR"
    fi
}

trap cleanup EXIT INT TERM


# ============================================================
# 标题
# ============================================================

echo
echo "=========================================="
echo "       i.MX6ULL SD Card 镜像制作"
echo "=========================================="
echo

echo "BASE_DIR  : $BASE_DIR"
echo "镜像文件   : $IMAGE_FILE"
echo "镜像大小   : ${IMAGE_SIZE_MB}MB"
echo
echo "U-Boot     : $UBOOT_IMX"
echo "Kernel     : $KERNEL_BIN"
echo "DTB        : $DTB_BIN"
echo "RootFS     : $ROOTFS_IMG"
echo


# ============================================================
# 1. 检查 root 权限
# ============================================================

echo "=========================================="
echo "1. 检查 root 权限"
echo "=========================================="

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: 请使用 root 权限运行此脚本"
    echo
    echo "例如："
    echo "    sudo ./make_image.sh"
    exit 1
fi

echo "OK: root 权限"
echo


# ============================================================
# 2. 检查输入文件
# ============================================================

echo "=========================================="
echo "2. 检查输入文件"
echo "=========================================="

for file in \
    "$UBOOT_IMX" \
    "$KERNEL_BIN" \
    "$DTB_BIN" \
    "$ROOTFS_IMG"
do
    if [ ! -f "$file" ]; then
        echo "ERROR: 文件不存在："
        echo "  $file"
        exit 1
    fi

    echo "OK: $file"
done

echo
echo "文件信息："

ls -lh \
    "$UBOOT_IMX" \
    "$KERNEL_BIN" \
    "$DTB_BIN" \
    "$ROOTFS_IMG"

echo


# ============================================================
# 3. 检查 rootfs archive 内容
# ============================================================

echo "=========================================="
echo "3. 检查 rootfs.cpio.gz"
echo "=========================================="

echo "RootFS archive："
echo "$ROOTFS_IMG"
echo

echo "检查关键文件："

gzip -dc "$ROOTFS_IMG" \
    | cpio -t \
    | grep -E \
'(^|/)(sbin/init|bin/sh|bin/busybox|lib/ld-linux-armhf.so.3|lib/ld-2.30.so|lib/libc.so.6|lib/libc-2.30.so|lib/libm.so.6|lib/libm-2.30.so|lib/libresolv.so.2|lib/libresolv-2.30.so)$' \
    || true

echo

echo "检查动态加载器和库："

TMP_LIST=$(mktemp)

gzip -dc "$ROOTFS_IMG" \
    | cpio -t > "$TMP_LIST"

REQUIRED_ARCHIVE_FILES=(
    "bin/busybox"
    "bin/sh"
    "sbin/init"
    "lib/ld-linux-armhf.so.3"
    "lib/ld-2.30.so"
    "lib/libc-2.30.so"
    "lib/libm-2.30.so"
    "lib/libresolv-2.30.so"
)

for item in "${REQUIRED_ARCHIVE_FILES[@]}"; do

    if grep -qx "$item" "$TMP_LIST"; then
        echo "OK: $item"
    else
        echo "ERROR: rootfs.cpio.gz 缺少：$item"
        rm -f "$TMP_LIST"
        exit 1
    fi

done

rm -f "$TMP_LIST"

echo
echo "rootfs.cpio.gz 检查通过"
echo


# ============================================================
# 4. 检查工具
# ============================================================

echo "=========================================="
echo "4. 检查工具"
echo "=========================================="

TOOLS="
dd
parted
losetup
mkfs.vfat
mkfs.ext4
mount
umount
gzip
cpio
blkid
find
cp
ls
df
fdisk
mktemp
readlink
basename
id
sleep
sync
"

for tool in $TOOLS; do

    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: 工具不存在：$tool"
        exit 1
    fi

    echo "OK: $tool"

done

echo


# ============================================================
# 5. 删除旧镜像
# ============================================================

echo "=========================================="
echo "5. 删除旧镜像"
echo "=========================================="

if [ -f "$IMAGE_FILE" ]; then

    echo "删除旧镜像："
    echo "$IMAGE_FILE"

    rm -f "$IMAGE_FILE"

fi

echo


# ============================================================
# 6. 创建镜像
# ============================================================

echo "=========================================="
echo "6. 创建 ${IMAGE_SIZE_MB}MB 镜像"
echo "=========================================="

dd \
    if=/dev/zero \
    of="$IMAGE_FILE" \
    bs=1M \
    count="$IMAGE_SIZE_MB" \
    status=progress

sync

echo
echo "镜像创建完成："
ls -lh "$IMAGE_FILE"

echo


# ============================================================
# 7. 创建 MBR 分区表
# ============================================================

echo "=========================================="
echo "7. 创建 MBR 分区表"
echo "=========================================="

parted -s "$IMAGE_FILE" mklabel msdos


# ------------------------------------------------------------
# P1
# ------------------------------------------------------------

echo
echo "创建 P1："
echo "  Start : 4MiB"
echo "  End   : 100MiB"
echo "  FS    : FAT32"

parted -s \
    "$IMAGE_FILE" \
    mkpart primary fat32 4MiB 100MiB


# ------------------------------------------------------------
# P2
# ------------------------------------------------------------

echo
echo "创建 P2："
echo "  Start : 100MiB"
echo "  End   : 600MiB"
echo "  FS    : ext4"

parted -s \
    "$IMAGE_FILE" \
    mkpart primary ext4 100MiB 600MiB


# ------------------------------------------------------------
# P3
# ------------------------------------------------------------

echo
echo "创建 P3："
echo "  Start : 600MiB"
echo "  End   : 1100MiB"
echo "  FS    : ext4"

parted -s \
    "$IMAGE_FILE" \
    mkpart primary ext4 600MiB 1100MiB

sync


# ============================================================
# 8. 查看分区表
# ============================================================

echo
echo "=========================================="
echo "8. 分区表"
echo "=========================================="

parted \
    "$IMAGE_FILE" \
    unit MiB \
    print

echo


# ============================================================
# 9. 写入 U-Boot
# ============================================================

echo "=========================================="
echo "9. 写入 U-Boot"
echo "=========================================="

echo "U-Boot："
echo "  文件   : $UBOOT_IMX"
echo "  偏移   : 1KiB"
echo "  bs     : 512"
echo "  seek   : 2"
echo

dd \
    if="$UBOOT_IMX" \
    of="$IMAGE_FILE" \
    bs=512 \
    seek=2 \
    conv=notrunc,fsync

sync

echo
echo "U-Boot 写入完成"
echo


# ============================================================
# 10. 建立 loop device
# ============================================================

echo "=========================================="
echo "10. 建立 loop device"
echo "=========================================="

LOOP_DEV=$(losetup -fP --show "$IMAGE_FILE")

if [ -z "$LOOP_DEV" ]; then
    echo "ERROR: 创建 loop device 失败"
    exit 1
fi

echo "LOOP_DEV = $LOOP_DEV"

sleep 1


# ============================================================
# 11. 检查分区设备
# ============================================================

echo
echo "检查分区设备："

for part in \
    "${LOOP_DEV}p1" \
    "${LOOP_DEV}p2" \
    "${LOOP_DEV}p3"
do

    if [ ! -b "$part" ]; then
        echo "ERROR: 分区设备不存在：$part"
        exit 1
    fi

    echo "OK: $part"

done

echo


# ============================================================
# 12. 创建临时工作目录
# ============================================================

echo "=========================================="
echo "12. 创建临时工作目录"
echo "=========================================="

WORK_DIR=$(mktemp -d)

BOOT_MOUNT="$WORK_DIR/boot"
ROOTFS_A_MOUNT="$WORK_DIR/rootfs_a"
ROOTFS_B_MOUNT="$WORK_DIR/rootfs_b"
ROOTFS_DIR="$WORK_DIR/rootfs"

mkdir -p \
    "$BOOT_MOUNT" \
    "$ROOTFS_A_MOUNT" \
    "$ROOTFS_B_MOUNT" \
    "$ROOTFS_DIR"

echo "WORK_DIR = $WORK_DIR"
echo


# ============================================================
# 13. 制作 P1 FAT32 Boot 分区
# ============================================================

echo "=========================================="
echo "13. 制作 P1 FAT32 Boot 分区"
echo "=========================================="

echo
echo "格式化 P1..."

mkfs.vfat \
    -F 32 \
    -n BOOT \
    "${LOOP_DEV}p1"

echo
echo "挂载 P1..."

mount \
    "${LOOP_DEV}p1" \
    "$BOOT_MOUNT"

echo
echo "复制 zImage..."

cp \
    "$KERNEL_BIN" \
    "$BOOT_MOUNT/zImage"

echo
echo "复制 DTB..."

cp \
    "$DTB_BIN" \
    "$BOOT_MOUNT/$DTB_FILENAME"

sync

echo
echo "P1 内容："

find "$BOOT_MOUNT" \
    -maxdepth 1 \
    -type f \
    -printf "%f %s bytes\n"

umount "$BOOT_MOUNT"

echo
echo "P1 完成"
echo


# ============================================================
# 14. 解压 rootfs.cpio.gz
# ============================================================

echo "=========================================="
echo "14. 解压 rootfs.cpio.gz"
echo "=========================================="

echo "RootFS 源文件："
echo "$ROOTFS_IMG"

echo
echo "开始解压..."

gzip -dc "$ROOTFS_IMG" \
    | cpio -idm \
    -D "$ROOTFS_DIR"

echo
echo "RootFS 解压完成"

echo
echo "RootFS 目录："

ls -la "$ROOTFS_DIR"

echo


# ============================================================
# 15. 检查 RootFS 关键文件
# ============================================================

echo "=========================================="
echo "15. 检查 RootFS"
echo "=========================================="

for dir in \
    bin \
    dev \
    etc \
    lib \
    proc \
    root \
    sbin \
    sys \
    usr
do

    if [ -e "$ROOTFS_DIR/$dir" ]; then
        echo "OK: $dir"
    else
        echo "WARNING: RootFS 中没有 $dir"
    fi

done

echo


echo "检查 /bin/sh："
ls -l "$ROOTFS_DIR/bin/sh"

echo

echo "检查 /bin/busybox："
ls -l "$ROOTFS_DIR/bin/busybox"

echo

echo "检查动态加载器："
ls -l "$ROOTFS_DIR/lib/ld-linux-armhf.so.3"
ls -l "$ROOTFS_DIR/lib/ld-2.30.so"

echo

echo "检查动态库："
ls -l "$ROOTFS_DIR/lib/libc-2.30.so"
ls -l "$ROOTFS_DIR/lib/libm-2.30.so"
ls -l "$ROOTFS_DIR/lib/libresolv-2.30.so"

echo

echo "检查动态加载器实际指向："

readlink -f \
    "$ROOTFS_DIR/lib/ld-linux-armhf.so.3"

echo


# ============================================================
# 16. 检查 BusyBox ELF
# ============================================================

echo "=========================================="
echo "16. 检查 BusyBox ELF"
echo "=========================================="

file "$ROOTFS_DIR/bin/busybox"

echo

readelf -l \
    "$ROOTFS_DIR/bin/busybox" \
    | grep interpreter \
    || true

echo

echo "BusyBox 依赖："

readelf -d \
    "$ROOTFS_DIR/bin/busybox" \
    | grep NEEDED \
    || true

echo


# ============================================================
# 17. 制作 P2 RootFS_A
# ============================================================

echo "=========================================="
echo "17. 制作 P2 RootFS_A"
echo "=========================================="

echo
echo "格式化 P2 为 ext4..."

mkfs.ext4 \
    -F \
    -L rootfs_a \
    "${LOOP_DEV}p2"

echo
echo "挂载 P2..."

mount \
    "${LOOP_DEV}p2" \
    "$ROOTFS_A_MOUNT"

echo
echo "复制 RootFS 到 P2..."

cp -a \
    "$ROOTFS_DIR/." \
    "$ROOTFS_A_MOUNT/"

sync

echo
echo "P2 RootFS_A 写入完成"

echo
echo "=========================================="
echo "检查 P2 动态加载器和库"
echo "=========================================="

ls -l "$ROOTFS_A_MOUNT/bin/sh"
ls -l "$ROOTFS_A_MOUNT/bin/busybox"

echo

ls -l "$ROOTFS_A_MOUNT/lib/ld-linux-armhf.so.3"
ls -l "$ROOTFS_A_MOUNT/lib/ld-2.30.so"

echo

ls -l "$ROOTFS_A_MOUNT/lib/libc-2.30.so"
ls -l "$ROOTFS_A_MOUNT/lib/libm-2.30.so"
ls -l "$ROOTFS_A_MOUNT/lib/libresolv-2.30.so"

echo

echo "P2 loader："
readlink -f \
    "$ROOTFS_A_MOUNT/lib/ld-linux-armhf.so.3"

echo

echo "P2 文件系统信息："
df -h "$ROOTFS_A_MOUNT"

umount "$ROOTFS_A_MOUNT"

echo
echo "P2 RootFS_A 完成"
echo


# ============================================================
# 18. 制作 P3 RootFS_B
# ============================================================

echo "=========================================="
echo "18. 制作 P3 RootFS_B"
echo "=========================================="

echo
echo "格式化 P3 为 ext4..."

mkfs.ext4 \
    -F \
    -L rootfs_b \
    "${LOOP_DEV}p3"

echo
echo "挂载 P3..."

mount \
    "${LOOP_DEV}p3" \
    "$ROOTFS_B_MOUNT"

echo
echo "复制 RootFS 到 P3..."

cp -a \
    "$ROOTFS_DIR/." \
    "$ROOTFS_B_MOUNT/"

sync

echo
echo "P3 RootFS_B 写入完成"

echo
echo "=========================================="
echo "检查 P3 动态加载器和库"
echo "=========================================="

ls -l "$ROOTFS_B_MOUNT/bin/sh"
ls -l "$ROOTFS_B_MOUNT/bin/busybox"

echo

ls -l "$ROOTFS_B_MOUNT/lib/ld-linux-armhf.so.3"
ls -l "$ROOTFS_B_MOUNT/lib/ld-2.30.so"

echo

ls -l "$ROOTFS_B_MOUNT/lib/libc-2.30.so"
ls -l "$ROOTFS_B_MOUNT/lib/libm-2.30.so"
ls -l "$ROOTFS_B_MOUNT/lib/libresolv-2.30.so"

echo

echo "P3 loader："
readlink -f \
    "$ROOTFS_B_MOUNT/lib/ld-linux-armhf.so.3"

echo

echo "P3 文件系统信息："
df -h "$ROOTFS_B_MOUNT"

umount "$ROOTFS_B_MOUNT"

echo
echo "P3 RootFS_B 完成"
echo


# ============================================================
# 19. 同步数据
# ============================================================

echo "=========================================="
echo "19. 同步数据"
echo "=========================================="

sync

echo "同步完成"
echo


# ============================================================
# 20. 获取分区 UUID
# ============================================================

echo "=========================================="
echo "20. 获取分区 UUID"
echo "=========================================="

P1_UUID=$(blkid -s UUID -o value "${LOOP_DEV}p1")
P2_UUID=$(blkid -s UUID -o value "${LOOP_DEV}p2")
P3_UUID=$(blkid -s UUID -o value "${LOOP_DEV}p3")

echo "P1 UUID: $P1_UUID"
echo "P2 UUID: $P2_UUID"
echo "P3 UUID: $P3_UUID"
echo


# ============================================================
# 21. 卸载 loop
# ============================================================

echo "=========================================="
echo "21. 卸载 loop device"
echo "=========================================="

sync

losetup -d "$LOOP_DEV"

LOOP_DEV=""

sync

echo "loop device 已卸载"
echo


# ============================================================
# 22. 删除临时目录
# ============================================================

if [ -n "$WORK_DIR" ] && [ -d "$WORK_DIR" ]; then
    rm -rf "$WORK_DIR"
fi

WORK_DIR=""


# ============================================================
# 23. 最终镜像检查
# ============================================================

echo "=========================================="
echo "23. 最终镜像检查"
echo "=========================================="

echo

echo "镜像路径："
echo "$IMAGE_FILE"

echo

echo "镜像大小："
ls -lh "$IMAGE_FILE"

echo

echo "MBR 分区表："

parted \
    "$IMAGE_FILE" \
    unit MiB \
    print

echo

echo "分区详细信息："

fdisk \
    -l "$IMAGE_FILE" \
    2>/dev/null \
    || true

echo


# ============================================================
# 24. 重新挂载镜像验证 P2/P3
# ============================================================

echo "=========================================="
echo "24. 最终验证镜像中的 RootFS"
echo "=========================================="

VERIFY_LOOP=$(losetup -fP --show "$IMAGE_FILE")

echo "VERIFY_LOOP = $VERIFY_LOOP"

sleep 1

VERIFY_MOUNT=$(mktemp -d)

echo
echo "挂载镜像 P2："

mount \
    "${VERIFY_LOOP}p2" \
    "$VERIFY_MOUNT"

echo
echo "检查镜像 P2："

echo "--- /bin/sh ---"
ls -l "$VERIFY_MOUNT/bin/sh"

echo
echo "--- /bin/busybox ---"
ls -l "$VERIFY_MOUNT/bin/busybox"

echo
echo "--- dynamic loader ---"
ls -l "$VERIFY_MOUNT/lib/ld-linux-armhf.so.3"
ls -l "$VERIFY_MOUNT/lib/ld-2.30.so"

echo
echo "--- dynamic libraries ---"
ls -l "$VERIFY_MOUNT/lib/libc-2.30.so"
ls -l "$VERIFY_MOUNT/lib/libm-2.30.so"
ls -l "$VERIFY_MOUNT/lib/libresolv-2.30.so"

echo
echo "--- loader target ---"

readlink -f \
    "$VERIFY_MOUNT/lib/ld-linux-armhf.so.3"

echo

echo "--- BusyBox ELF ---"

file "$VERIFY_MOUNT/bin/busybox"

echo

readelf -l \
    "$VERIFY_MOUNT/bin/busybox" \
    | grep interpreter \
    || true

echo

umount "$VERIFY_MOUNT"

rm -rf "$VERIFY_MOUNT"

losetup -d "$VERIFY_LOOP"

sync

echo


# ============================================================
# 25. 最终完成
# ============================================================

echo "=========================================="
echo "       镜像制作完成"
echo "=========================================="

echo

echo "镜像文件："
echo "  $IMAGE_FILE"

echo

echo "分区布局："

echo
echo "  P1 : FAT32"
echo "       /zImage"
echo "       /$DTB_FILENAME"

echo
echo "  P2 : ext4"
echo "       RootFS_A"

echo
echo "  P3 : ext4"
echo "       RootFS_B"

echo
echo "U-Boot："
echo "  offset = 1KiB"

echo

echo "RootFS 动态加载器："
echo "  /lib/ld-linux-armhf.so.3"
echo "  /lib/ld-2.30.so"

echo

echo "RootFS 动态库："
echo "  /lib/libc-2.30.so"
echo "  /lib/libm-2.30.so"
echo "  /lib/libresolv-2.30.so"

echo

echo "=========================================="
echo "       ALL CHECKS PASSED"
echo "=========================================="





# export PATH=/home/xingxinliao/toolchain/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin:$PATH
# export ARCH=arm
# export CROSS_COMPILE=arm-none-linux-gnueabihf-

# arm-none-linux-gnueabihf-gcc --version
# sudo dd if=/dev/zero of=/dev/sdb bs=512 count=8192 conv=fsync
# sync

# sudo dd if=factory_emmc_user.img of=/dev/sdb bs=4M status=progress conv=fsync
# sync

# mmc dev 0
# fatload mmc 0:1 0x80800000 zImage
# fatload mmc 0:1 0x83000000 imx6ull-mmc-npi.dtb
# setenv bootargs 'console=ttymxc0,115200 root=/dev/mmcblk0p3 rootfstype=ext4 rw'
# bootz 0x80800000 - 0x83000000


# ip addr flush dev eth0
# ip addr add 192.168.30.121/24 dev eth0
# ip link set eth0 up
# ip route add default via 192.168.30.1 dev eth0
# mount -t nfs -o nolock 192.168.30.111:/home/xingxinliao/update/system_img/results /tmp/



# mkdir -p /etc
# cat > /etc/fw_env.config <<'EOF'
# /dev/mmcblk0 0xC0000 0x2000
# EOF
# cp /temp/fw_printenv /usr/bin/
# mkdir -p /var/lock
# ln -s /usr/bin/fw_printenv /usr/bin/fw_setenv


# cp tools/env/fw_printenv /home/xingxinliao/update/system_img/results/


# fw_setenv bootcmd "mmc dev 0; fatload mmc 0:1 0x80800000 zImage; fatload mmc 0:1 0x83000000 imx6ull-mmc-npi.dtb; setenv bootargs 'console=ttymxc0,115200 root=/dev/mmcblk0p3 rootfstype=ext4 rw'; bootz 0x80800000 - 0x83000000"
# mmc dev 0; fatload mmc 0:1 0x80800000 zImage; fatload mmc 0:1 0x83000000 imx6ull-mmc-npi.dtb; setenv bootargs 'console=ttymxc0,115200 root=/dev/mmcblk0p3 rootfstype=ext4 rw'
# bootz 0x80800000 - 0x83000000

#  fw_printenv env1111
