# Lenovo ZHAOYANG K27 触控板竖椭圆修复（AlpsPS/2 ALPS GlidePoint）

> 一台 2011 年的联想昭阳 K27，在 Arch Linux + KDE Plasma Wayland 下触控板
> **竖向移动明显比横向快、画圈变成竖椭圆**。本仓库记录完整诊断过程、修复
> 参数、校准工具和可复现的教程，供他人参考及日后重装使用。
>
> 修复只需要一个 libinput quirk 文件，不修改内核、不需要 DKMS、重启会话即生效。

## 症状

- 触控板上画一个物理上的圆，屏幕上的光标轨迹是**竖长的椭圆**（本机约高 7%）。
- 竖向移动一小段，光标走得比横向同样距离远。
- 触控板其余功能正常（点击、双指滚动、轻拍等）。

## 适用环境

| 项目 | 值 |
|---|---|
| 机型 | Lenovo ZHAOYANG K27（昭阳 K27，2011，Legacy BIOS） |
| 系统 | Arch Linux（kernel `7.2.14-zen1-1-zen`，libinput `1.32.0`） |
| 桌面 | KDE Plasma **Wayland**（kwin_wayland） |
| 触控板 | `AlpsPS/2 ALPS GlidePoint`，PS/2（i8042 serio4），semi-MT |
| 设备 ID | bus `0x0011`(i8042) / vendor `0x0002` / product `0x0008` / version `0x0500` |

> 这套方法适用于**任何内核未报告分辨率、且 X/Y 轴物理比例与坐标范围不成比例**
> 的触控板，不限于 ALPS 或这台机器。

## 根因（一句话版）

内核 ALPS 串口驱动给 ABS_X/Y（以及 ABS_MT_POSITION_X/Y）报告的
`resolution = 0`，libinput 只能给两轴设**相同**的假分辨率（1 unit/mm），
于是坐标范围 `1024 x 448` 被当成等比例处理；而触控板实际是宽扁的，
所以竖向被放大，画圈成竖椭圆。

修复就是通过 libinput quirk 的 `AttrSizeHint=宽x高`（单位 mm）告诉 libinput
触控板的物理尺寸，让它为两轴算出**不同的** units/mm，从而在 mm 空间里
等比例还原运动。

## 安装（重装系统后照做）

```bash
# 1. 放 quirk 文件
sudo mkdir -p /etc/libinput
sudo cp quirk/local-overrides.quirks /etc/libinput/local-overrides.quirks
sudo chmod 644 /etc/libinput/local-overrides.quirks

# 2. 重启 KWin（或直接注销重登）
systemctl --user restart plasma-kwin_wayland

# 3. 验证：应显示 Size: 73x37mm（而不是几千毫米或 102x56mm）
sudo libinput list-devices | grep -A8 GlidePoint

# 4. 验证 quirk 命中
sudo libinput quirks list /dev/input/event11
# 期望输出：
# ModelALPSSerialTouchpad=1
# AttrSizeHint=72x36
```

> `libinput` 命令来自 `libinput-tools` 包（仅验证需要，修复本身不需要）。
> 设备号 `event11` 可能变化，用 `libinput list-devices` 查实际路径。

## 校准（换机器/换参数时用）

仓库里的 [`circletest.html`](circletest.html) 是一个**完全离线**的测量页：

1. 用任意浏览器打开它（`file:///path/to/circletest.html`）；
2. 按住鼠标左键，在触控板上**慢慢画一个物理圆**，松开；
3. 页面显示轨迹的 **竖向长度 ÷ 横向长度**：
   - `1.00` 左右 → 已经是正圆；
   - `> 1.03` → 竖向偏快（需要增大 `res_x/res_y`，即**减小**高度值）；
   - `< 0.97` → 竖向偏慢/横向偏快（**增大**高度值）。

### 关键：libinput 的分辨率是整数

libinput 用 `resolution = 坐标范围 / AttrSizeHint` 计算，并**截断为整数**
units/mm。本机坐标范围是 `1024 x 448`，所以可实现的 `res_x/res_y` 是离散的：

| AttrSizeHint | res_x = 1024/W | res_y = 448/H | res_x/res_y | 等效圆比例 R |
|---|---|---|---|---|
| 无 quirk（默认假设 69×50mm） | 14 | 8 | 1.750 | ≈1.50（竖快约 50%） |
| `100x55`（第一次尝试） | 10 | 8 | 1.250 | 1.07（竖快 7%） |
| `100x51` | 10 | **8** | 1.250 | 1.07（**无效**！448/51 仍截断为 8） |
| `72x36`（本机最终） | 14 | 12 | **1.1667** | **≈1.00** |
| `74x38` | 13 | 11 | 1.1818 | ≈1.01（竖快 1.2%） |
| `65x33` | 15 | 13 | 1.1538 | ≈0.99（竖慢 1.2%） |

> 注意 `100x51` 这一行：这是本仓库踩过的坑——改小高度不一定改变整数分辨率，
> 必须跨过 `448/H` 的整数台阶才有用。

### 通用调参公式

在某个已知参数下测得比例 `R`（竖向÷横向），当前整数分辨率为 `rx/ry`，
则目标分辨率为：

```
目标 rx/ry = (rx/ry) / R
```

然后选 `W`、`H` 使 `floor(1024/W) / floor(448/H)` 最接近该目标。
**比例只由 `res_x/res_y` 决定，与绝对大小无关**；`W`、`H` 同时乘同一个系数
只改变整体指针速度（数值越大指针越慢），不改变圆形状。整体速度也可以直接
用「系统设置 → 触控板/鼠标 → 指针速度」滑块补偿。

## 验证与回滚

```bash
# 查看 quirk 是否命中（--verbose 会列出每个匹配条件）
sudo libinput quirks list --verbose /dev/input/event11 | grep -E "local-overrides|AttrSizeHint"

# 回滚：删除文件并重启 KWin
sudo rm /etc/libinput/local-overrides.quirks
systemctl --user restart plasma-kwin_wayland
```

## 目录结构

```
.
├── README.md                      # 本文件：症状、原理、安装、校准
├── circletest.html                # 离线圆形比例测量页
├── quirk/
│   └── local-overrides.quirks     # 最终可用的 libinput quirk
├── docs/
│   ├── diagnosis.md               # 深入诊断过程与源码级原理
│   └── edge-on-arch-slow-network.md  # 顺带记录：慢网络下装 Edge 的教训
├── scripts/
│   ├── install.sh                 # 一键安装 quirk
│   └── absinfo.c                  # 读取设备坐标范围/分辨率的工具
├── PROMPTS.md                     # 可复用的 AI 诊断提示词
└── LICENSE                        # MIT
```

## 快速自检清单（重装后）

- [ ] `libinput list-devices` 里触控板 `Size` 是 `73x37mm` 左右
- [ ] `libinput quirks list` 有 `AttrSizeHint=72x36`
- [ ] `circletest.html` 画圆比值在 0.97~1.03
- [ ] 双指滚动、轻拍点击正常

## 相关

- 详细推导与源码分析：[docs/diagnosis.md](docs/diagnosis.md)
- 慢网络下装 Edge 的完整踩坑记录：[docs/edge-on-arch-slow-network.md](docs/edge-on-arch-slow-network.md)
- AI 诊断提示词模板：[PROMPTS.md](PROMPTS.md)

## License

MIT
