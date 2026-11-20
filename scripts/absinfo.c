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

/* 常见 ABS 轴代码的名字（linux/input-event-codes.h）；未列出的返回 NULL。 */
static const char *abs_name(int code)
{
    switch (code) {
    case ABS_X:              return "ABS_X";
    case ABS_Y:              return "ABS_Y";
    case ABS_Z:              return "ABS_Z";
    case ABS_RX:             return "ABS_RX";
    case ABS_RY:             return "ABS_RY";
    case ABS_RZ:             return "ABS_RZ";
    case ABS_WHEEL:          return "ABS_WHEEL";
    case ABS_PRESSURE:       return "ABS_PRESSURE";
    case ABS_DISTANCE:       return "ABS_DISTANCE";
    case ABS_TILT_X:         return "ABS_TILT_X";
    case ABS_TILT_Y:         return "ABS_TILT_Y";
    case ABS_TOOL_WIDTH:     return "ABS_TOOL_WIDTH";
    case ABS_VOLUME:         return "ABS_VOLUME";
    case ABS_MISC:           return "ABS_MISC";
    case ABS_MT_SLOT:        return "ABS_MT_SLOT";
    case ABS_MT_TOUCH_MAJOR: return "ABS_MT_TOUCH_MAJOR";
    case ABS_MT_TOUCH_MINOR: return "ABS_MT_TOUCH_MINOR";
    case ABS_MT_WIDTH_MAJOR: return "ABS_MT_WIDTH_MAJOR";
    case ABS_MT_WIDTH_MINOR: return "ABS_MT_WIDTH_MINOR";
    case ABS_MT_ORIENTATION: return "ABS_MT_ORIENTATION";
    case ABS_MT_POSITION_X:  return "ABS_MT_POSITION_X";
    case ABS_MT_POSITION_Y:  return "ABS_MT_POSITION_Y";
    case ABS_MT_TOOL_TYPE:   return "ABS_MT_TOOL_TYPE";
    case ABS_MT_BLOB_ID:     return "ABS_MT_BLOB_ID";
    case ABS_MT_TRACKING_ID: return "ABS_MT_TRACKING_ID";
    case ABS_MT_PRESSURE:    return "ABS_MT_PRESSURE";
    case ABS_MT_DISTANCE:    return "ABS_MT_DISTANCE";
    case ABS_MT_TOOL_X:      return "ABS_MT_TOOL_X";
    case ABS_MT_TOOL_Y:      return "ABS_MT_TOOL_Y";
    default:                 return NULL;
    }
}

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

    /* 设备名：确认没打开错 event 节点。EVIOCGNAME 最多写 size 字节，
     * 这里传 sizeof-1，缓冲区最后一个字节保持 0，保证字符串终止。 */
    char name[256] = "(EVIOCGNAME 失败：这可能不是 input 设备)";
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0)
        fprintf(stderr, "警告：%s 无法读取设备名: %s\n", argv[1], strerror(errno));
    printf("%s: %s\n", argv[1], name);

    for (int a = 0; a < ABS_CNT; a++) {
        struct input_absinfo ai;
        if (ioctl(fd, EVIOCGABS(a), &ai) == 0 && ai.maximum > ai.minimum) {
            const char *n = abs_name(a);
            char label[48];
            snprintf(label, sizeof(label), "ABS_%d (%s)", a, n ? n : "未知");
            printf("%-30s min=%-6d max=%-6d range=%-6d fuzz=%-3d flat=%-3d res=%d\n",
                   label, ai.minimum, ai.maximum,
                   ai.maximum - ai.minimum, ai.fuzz, ai.flat, ai.resolution);
        }
    }

    close(fd);
    return 0;
}
