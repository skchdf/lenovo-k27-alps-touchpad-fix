# 诊断笔记：ALPS 触控板为什么"竖快横慢"

本文记录从现象到根因的完整推导，包括 libinput 源码中的具体函数与整数截断的坑。
即使不是这台机器，只要是"内核不报分辨率"的触控板，诊断思路同样适用。

## 1. 现场数据

### 设备

```
$ grep -A9 "GlidePoint" /proc/bus/input/devices
N: Name="AlpsPS/2 ALPS GlidePoint"
P: Phys=isa0060/serio4/input0
S: Sysfs=/devices/platform/i8042/serio4/input/input17
H: Handlers=event11 mouse1
B: PROP=9
B: EV=b
```

- PS/2 接口（i8042，`serio4`），semi-MT（`PROP=9` 表示
  `INPUT_PROP_BUTTONPAD` 之外还有半多点能力）。
- `vendor=0x0002 product=0x0008`，即 ALPS 串口触控板。

### 坐标轴与分辨率

用 `scripts/absinfo.c`（直接 `EVIOCGABS` ioctl）读取：

```
ABS_0  (ABS_X):               min=0 max=1024 range=1024 res=0
ABS_1  (ABS_Y):               min=0 max=448  range=448  res=0
ABS_24 (ABS_PRESSURE):        min=0 max=127  range=127  res=0
ABS_47 (ABS_MT_SLOT):         min=0 max=3    range=3    res=0
ABS_53 (ABS_MT_POSITION_X):   min=0 max=1024 range=1024 res=0
ABS_54 (ABS_MT_POSITION_Y):   min=0 max=448  range=448  res=0
ABS_57 (ABS_MT_TRACKING_ID):  min=0 max=65535           res=0
```

关键点：

1. **所有轴的 `resolution` 都是 0** —— 内核没有告诉用户空间"1 单位 = 多少毫米"。
2. X 范围 1024、Y 范围 448，比例 2.286:1，但这**只是坐标范围**，不等于物理尺寸。

### 软件版本

```
kernel 7.2.14-zen1-1-zen
libinput 1.32.0-1
KDE Plasma Wayland (kwin_wayland)
```

## 2. libinput 的处理链

### 2.1 如果内核报了分辨率

libinput 在 `evdev_fix_abs_resolution()`（`src/evdev.c`）里直接采用，不做任何猜测。
这是最理想的情况：每个轴都有真实的 units/mm，运动自然等比。

### 2.2 内核没报分辨率时

同一函数（libinput 1.32.0 源码）：

```c
size_t xres = EVDEV_FAKE_RESOLUTION, yres = EVDEV_FAKE_RESOLUTION;

/* 若 udev 提供了 LIBINPUT_ATTR_RESOLUTION_HINT，用它；
   否则若提供了 LIBINPUT_ATTR_SIZE_HINT（宽x高，mm），则
   xres = 轴范围 / 宽，yres = 轴范围 / 高； */
if (!evdev_read_attr_res_prop(device, &xres, &yres) &&
    evdev_read_attr_size_prop(device, &widthmm, &heightmm)) {
        xres = absinfo_range(absx) / widthmm;
        yres = absinfo_range(absy) / heightmm;
}

libevdev_set_abs_resolution(evdev, xcode, xres);
libevdev_set_abs_resolution(evdev, ycode, yres);
```

其中 `#define EVDEV_FAKE_RESOLUTION 1`（`src/evdev.h`）。
注意 `xres`/`yres` 是 `size_t`，**除法是整数除法，直接截断**。这就是本仓库
最大的坑：`AttrSizeHint` 的毫米值必须跨过"整数台阶"才有效。

### 2.3 默认兜底：假设 69×50mm

如果既没有内核分辨率、也没有任何 hint，libinput 还有最后一道兜底
（`src/evdev-mt-touchpad.c`，`tp_init_default_resolution()`）：

```c
const int touchpad_width_mm = 69, /* 1 under palm detection */
          touchpad_height_mm = 50;
...
xres = device->abs.dimensions.x / touchpad_width_mm;
yres = device->abs.dimensions.y / touchpad_height_mm;
```

也就是说：**没有 quirk 时，libinput 把这台机器的触控板当成 69×50mm**，
于是

```
xres = 1024 / 69 = 14.84 -> 14 units/mm
yres =  448 / 50 =  8.96 ->  8 units/mm
```

两轴分辨率之比 `14/8 = 1.75`，即竖向每单位被当成更短的物理距离，
光标竖向走得比横向快得多。

### 2.4 分辨率如何变成光标位移

`tp_init_accel()`（`src/evdev-mt-touchpad.c`）：

```c
res_x = tp->device->abs.absinfo_x->resolution;
res_y = tp->device->abs.absinfo_y->resolution;

/*
 * Not all touchpads report the same amount of units/mm (resolution).
 * Normalize motion events to the default mouse DPI as base
 * (unaccelerated) speed. This also evens out any differences in x
 * and y resolution, so that a circle on the touchpad does not turn
 * into an ellipse on the screen.
 */
tp->accel.x_scale_coeff = (DEFAULT_MOUSE_DPI / 25.4) / res_x;
tp->accel.y_scale_coeff = (DEFAULT_MOUSE_DPI / 25.4) / res_y;
```

源码注释已经把目的写得很清楚：**用两轴各自的 units/mm 把运动归一化到毫米，
圆才不会变成椭圆**。所以只要 `res_x != res_y` 且不等于真实值，椭圆就会出现。

## 3. 数值推导

设触控板真实物理宽高为 `W_true`、`H_true`（mm），坐标范围 `Rx=1024, Ry=448`。
手指在触控板上画一个直径 `d` mm 的物理圆：

- 横向原始单位增量：`d * Rx / W_true`
- 纵向原始单位增量：`d * Ry / H_true`
- 光标位移：`dx ∝ (d*Rx/W_true)/res_x`，`dy ∝ (d*Ry/H_true)/res_y`

所以屏幕上的"高÷宽"比例：

```
R = dy/dx = (Ry/H_true)/(Rx/W_true) * (res_x/res_y)
          = (448/1024) * (W_true/H_true) * (res_x/res_y)
          = 0.4375 * (W_true/H_true) * (res_x/res_y)
```

### 3.1 用实测值反推真实宽高比

先装上 `AttrSizeHint=100x55` 时：

```
res_x = 1024/100 = 10.24 -> 10
res_y =  448/55  =  8.14 ->  8
res_x/res_y = 1.25
```

用 `circletest.html` 实测 `R = 1.07`，代入：

```
1.07 = 0.4375 * (W_true/H_true) * 1.25
W_true/H_true = 1.07 / 0.546875 = 1.9566
```

即真实触控板约为 **1.96:1** 的宽扁形状（约 73×37mm）。

### 3.2 求目标分辨率

要 `R = 1`：

```
res_x/res_y = (1024/448) / (W_true/H_true)
            = 2.2857 / 1.9566
            = 1.168
```

在整数分辨率下枚举，最接近的是 `14/12 = 1.1667`（误差 0.1%），
对应：

```
1024 / W = 14  -> W 在 [69, 73] 之间
 448 / H = 12  -> H 在 [35, 37] 之间
```

取整数值 `72x36` 写入 quirk，`libinput list-devices` 显示
`Size: 73x37mm`，与推导出的真实尺寸吻合。

预测比例：

```
R = 0.4375 * 1.9566 * (14/12) = 0.9987 ≈ 1.00
```

### 3.3 完整对照表

| 状态 | AttrSizeHint | res_x | res_y | res_x/res_y | 预测/实测 R |
|---|---|---|---|---|---|
| 无 quirk（默认 69×50） | — | 14 | 8 | 1.750 | ≈1.50（竖快 50%） |
| 第一次尝试 | 100x55 | 10 | 8 | 1.250 | **实测 1.07** |
| 无效调整 | 100x51 | 10 | **8** | 1.250 | 1.07（完全没变） |
| 最终 | 72x36 | 14 | 12 | 1.1667 | 预测 1.00 |

## 4. 为什么 `100x51` 无效

`448 / 51 = 8.784`，整数截断后仍是 **8**，与 `448/55 = 8.145 -> 8` 相同。
所以那次修改对形状没有任何影响。这是最容易踩的坑：**高度值必须跨过
`448/H` 的整数台阶**（H 从 55 改到 51 恰好没有跨过 8→9 的边界）。

同理，宽度也有台阶：`1024/W` 的整数跳变发生在
`W = 1024/n` 附近（n=8..20）。

## 5. 验证方法

```bash
# 1. quirk 是否命中（--verbose 会显示每个 Match 条件）
sudo libinput quirks list --verbose /dev/input/event11 | grep -E "local-overrides|AttrSizeHint"
# 期望：property added: AttrSizeHint from [ALPS K27 GlidePoint Size Hint]

# 2. 设备尺寸（libinput 用整数分辨率反算出的尺寸）
sudo libinput list-devices | grep -A6 GlidePoint
# 期望：Size: 73x37mm

# 3. 手感/形状：用 circletest.html 画圆，比值应在 0.97~1.03
```

## 6. 换一台机器怎么做

1. 读范围与分辨率：

   ```bash
   gcc -O2 -o absinfo scripts/absinfo.c
   sudo ./absinfo /dev/input/eventN
   ```

2. 若 `res` 已经是非 0：不需要修，问题在别处（可能是桌面环境的指针加速）。
3. 若 `res` 为 0：先按本文档装一个粗略的 `AttrSizeHint`，用 `circletest.html`
   测出 `R`，再用第 3 节的公式反推真实宽高比和目标 `res_x/res_y`。
4. 枚举满足 `floor(Rx/W)/floor(Ry/H) ≈ 目标` 的整数 `W,H`，写入 quirk，
   重启 KWin（或重登）验证。

## 7. 其他可行方案（为什么选 quirk）

| 方案 | 说明 | 评价 |
|---|---|---|
| libinput quirk `AttrSizeHint` | 本仓库采用，`/etc/libinput/local-overrides.quirks` | ✅ 简洁、持久、Wayland/X11 都生效 |
| udev hwdb / `LIBINPUT_ATTR_SIZE_HINT` | 需要写 hwdb 规则并 `systemd-hwdb update` | 可行但更繁琐 |
| `LIBINPUT_ATTR_RESOLUTION_HINT` | 直接给两轴分辨率，绕过尺寸换算 | 可用，但可读性差、同样受整数限制 |
| Xorg `Option "Resolution"` | 仅 X11 的 synaptics/libinput 驱动 | ❌ Wayland 无效 |
| 改内核 alps.c 上报真实分辨率 | 最彻底 | ❌ 需维护补丁，不值得 |

## 8. 一句话总结

> 内核说"我不知道 1 单位是多少毫米"，libinput 只好猜（默认 69×50mm 或你给的
> size hint）；猜出来的两轴 units/mm 不相等，圆就成了椭圆。
> 把 `AttrSizeHint` 调到让 `floor(1024/W) : floor(448/H)` 等于真实宽高比，
> 圆就正了。
