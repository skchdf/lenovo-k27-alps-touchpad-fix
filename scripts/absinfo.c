/*
 * absinfo - 打印一个 evdev 设备所有 ABS 轴的范围与分辨率
 *
 * 编译：gcc -O2 -o absinfo absinfo.c
 * 用法：sudo ./absinfo /dev/input/eventN
 *
 * 诊断触控板"圆变椭圆"时，关键看 ABS_X/ABS_Y（以及
 * ABS_MT_POSITION_X/Y）的 resolution 是否为 0：
 *   0  -> 内核没告诉用户空间 units/mm，libinput 只能猜
 *   非0 -> 内核已提供，问题多半不在分辨率
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "用法: %s /dev/input/eventN\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "打开 %s 失败: %s（需要 root 或 input 组权限）\n",
                argv[1], strerror(errno));
        return 1;
    }

    printf("%s\n", argv[1]);
    for (int a = 0; a < ABS_CNT; a++) {
        struct input_absinfo ai;
        if (ioctl(fd, EVIOCGABS(a), &ai) == 0 && ai.maximum > ai.minimum) {
            printf("ABS_%-5d min=%-6d max=%-6d range=%-6d fuzz=%-3d flat=%-3d res=%d\n",
                   a, ai.minimum, ai.maximum,
                   ai.maximum - ai.minimum, ai.fuzz, ai.flat, ai.resolution);
        }
    }

    close(fd);
    return 0;
}
