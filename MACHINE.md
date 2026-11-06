# 本机参数（Lenovo ZHAOYANG K27）

> 最后核实：**2026-11-06**
> 用途：重装系统后照本文件直接恢复触控板修复，**不需要重新测量**。
> 若硬件或内核大变（例如换了触控板、内核坐标范围变了），再按
> [docs/diagnosis.md](docs/diagnosis.md) 重新推导。

## 1. 整机

| 项目 | 值 |
|---|---|
| 型号 | Lenovo ZHAOYANG K27（昭阳 K27，2011） |
| 主板 / BIOS | `KLx` / `46CN21WW`，Legacy BIOS（无 UEFI） |
| DMI modalias | `dmi:bvnLENOVO:bvr46CN21WW:bd09/07/2011:svnLENOVO:pnZHAOYANGK27:pvrRev1.0:rvnLENOVO:rnKLx:rvrRev1.0:cvnLENOVO:ct10:cvrRev1.0:sku:pfa:` |
| CPU | Intel Pentium B970 @ 2.30 GHz（2 核 2 线程，Sandy Bridge） |
| 内存 | 3.7 GiB + zram0（1.8 G swap） |
| 显卡 | Intel Sandy Bridge 核显（mesa / libva-intel-driver） |
| 磁盘 | `/dev/sda` 298.1 G：`sda1` 148.1G NTFS、`sda3` 40G ext4、`sda4` 10G ext4（data）、`sda5` 100G ext4（`/`） |
| 系统 | Arch Linux，kernel `7.2.14-zen1-1-zen`，KDE Plasma `6.8.2`（Wayland / kwin_wayland） |
| libinput | `1.32.0-1` |
| 输入设备组 | `LIBINPUT_DEVICE_GROUP=11/2/8:isa0060/serio4` |

## 2. 触控板设备

| 项目 | 值 |
|---|---|
| 名称 | `AlpsPS/2 ALPS GlidePoint` |
| 设备节点 | `/dev/input/event11`（**编号可能随重装/内核变化**，用 `libinput list-devices` 查） |
| 物理路径 | `isa0060/serio4/input0`（PS/2 / i8042） |
| ID | bus `0x0011`（i8042） / vendor `0x0002` / product `0x0008` / version `0x0500` |
| 能力 | `PROP=9`（semi-MT），`Handlers=event11 mouse1` |

### 坐标轴（`EVIOCGABS` 实测）

| 轴 | 含义 | min | max | range | resolution |
|---|---|---|---|---|---|
| `ABS_0` | ABS_X | 0 | 1024 | 1024 | **0** |
| `ABS_1` | ABS_Y | 0 | 448 | 448 | **0** |
| `ABS_24` | ABS_PRESSURE | 0 | 127 | 127 | 0 |
| `ABS_47` | ABS_MT_SLOT | 0 | 3 | 3 | 0 |
| `ABS_53` | ABS_MT_POSITION_X | 0 | 1024 | 1024 | **0** |
| `ABS_54` | ABS_MT_POSITION_Y | 0 | 448 | 448 | **0** |
| `ABS_57` | ABS_MT_TRACKING_ID | 0 | 65535 | 65535 | 0 |

> 关键：X/Y（含 MT）**两轴 resolution 均为 0**，这是需要 quirk 的唯一原因。

## 3. 修复参数（本机最终值）

```ini
[ALPS K27 GlidePoint Size Hint]
MatchUdevType=touchpad
MatchBus=ps2
MatchVendor=0x0002
MatchProduct=0x0008
MatchDMIModalias=dmi:*svnLENOVO:*pnZHAOYANGK27*
AttrSizeHint=72x36
```

### 计算值

| 项目 | 值 |
|---|---|
| `AttrSizeHint` | **72 x 36 mm** |
| `res_x = floor(1024/72)` | **14** units/mm |
| `res_y = floor(448/36)` | **12** units/mm |
| `res_x/res_y` | **1.1667** |
| libinput 显示 Size | **73 x 37 mm** |
| 推导真实宽高比 | **1.9566 : 1**（约 73×37 mm） |
| 预测圆比例（高÷宽） | **0.999 ≈ 1.00** |

### 调参历史（重要，避免重复踩坑）

| 参数 | res_x | res_y | 比例 | 实测/预测 |
|---|---|---|---|---|
| 无 quirk（libinput 默认假设 69×50mm） | 14 | 8 | 1.750 | ≈1.50（竖快约 50%） |
| `100x55` | 10 | 8 | 1.250 | **实测 1.07**（竖快 7%） |
| `100x51` | 10 | **8** | 1.250 | 1.07（**无效**，整数截断没跨台阶） |
| **`72x36`（当前）** | **14** | **12** | **1.1667** | **预测 1.00** |

## 4. 当前 libinput 输出（对照用）

```
Device:                  AlpsPS/2 ALPS GlidePoint
Kernel:                  /dev/input/event11
Id:                      serial:0002:0008
Group:                   10
Seat:                    seat0, default
Size:                    73x37mm
Capabilities:            pointer
Tap-to-click:            disabled
Tap-and-drag:            enabled
Tap button map:          left/right/middle
Tap drag lock:           disabled
Left-handed:             disabled
Nat.scrolling:           disabled
Middle emulation:        enabled
Calibration:             n/a
Scroll methods:          *two-finger edge
Scroll button:           n/a
Scroll button lock:      n/a
Click methods:           none
Clickfinger button map:  n/a
Disable-w-typing:        enabled
Disable-w-trackpointing: enabled
Accel profiles:          flat *adaptive custom
Rotation:                n/a
Area rectangle:          n/a
```

```
$ sudo libinput quirks list /dev/input/event11
ModelALPSSerialTouchpad=1
AttrSizeHint=72x36
```

`/etc/libinput/local-overrides.quirks` 的 MD5（2026-11-06）：

```
9f49020a16397e35c793d61b833d42fc
```

## 5. 重装系统后恢复（不用测量）

```bash
# 1. 拉仓库
git clone https://github.com/skchdf/lenovo-k27-alps-touchpad-fix.git
cd lenovo-k27-alps-touchpad-fix

# 2. 一键安装（内部会 sudo 复制 quirk 并重启 KWin）
./scripts/install.sh

# 3. 确认（应显示 Size: 73x37mm）
sudo libinput list-devices | grep -A6 GlidePoint
sudo libinput quirks list /dev/input/event11
```

或者手动：

```bash
sudo install -Dm644 quirk/local-overrides.quirks /etc/libinput/local-overrides.quirks
systemctl --user restart plasma-kwin_wayland
```

### 验收标准（不用画圆）

- `libinput list-devices` 的 `Size` 为 **73x37mm**；
- `libinput quirks list` 输出 `AttrSizeHint=72x36`；
- 两条都满足即说明参数已生效。手感/圆度只是最终确认，可选。

## 6. 本机特有的注意事项

1. **整数截断**：`res = floor(range / hint)`。改 H 时必须跨过
   `448/H` 的整数台阶（例如 55→51 仍是 8，属于无效修改）。
2. **DMI 匹配**：`MatchDMIModalias=dmi:*svnLENOVO:*pnZHAOYANGK27*`
   依赖机型字符串 `ZHAOYANGK27`。换主板/换机型要改这一行。
3. **设备号**：`event11` 可能变，所有命令以 `libinput list-devices`
   输出的 `Kernel:` 为准。
4. **坐标范围**：本机是 `1024x448`。如果哪天内核升级后范围变了
   （用 `scripts/absinfo` 复查），`72x36` 需要按新范围重算。
5. **整体速度**：形状只由 `res_x/res_y` 决定。若觉得指针整体偏快/偏慢，
   用「系统设置 → 触控板 → 指针速度」调，或把 `72x36` 同比例改成
   `144x72`（更慢）等，形状不变。
6. **Wayland 生效方式**：重启 `plasma-kwin_wayland` 即可；X11 下重登即可。
