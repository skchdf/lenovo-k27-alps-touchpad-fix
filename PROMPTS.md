# 可复用的 AI 诊断提示词

以下提示词可以直接粘贴给有终端权限的 AI 助手（Claude Code、OpenCode、Codex 等），
用来复现本仓库的诊断与调参过程。把 `<...>` 替换成你的实际信息。

---

## 1. 初诊：判断是不是"分辨率缺失"导致的比例失真

```text
我在 <发行版 + 桌面环境> 上遇到触控板问题：
<设备名，例如 AlpsPS/2 ALPS GlidePoint>，症状是
<竖向移动比横向快 / 画圈变竖椭圆 / 横向太快>。

请按以下步骤诊断，不要先改配置，先收集证据：

1. 用 `grep -A9 -i "<设备名关键词>" /proc/bus/input/devices` 确认设备路径、
   bus/vendor/product、PROP 能力。
2. 写一个小的 C 程序（或使用 evtest）读取该 event 设备的 ABS 轴信息，
   列出每个轴的 min/max/range/resolution。
   重点看 ABS_X/ABS_Y（以及 ABS_MT_POSITION_X/Y）的 resolution 是否为 0。
3. 检查 `libinput --version`、内核版本、桌面是否 Wayland。
4. 根据结果判断：
   - 如果 resolution 非 0：问题不在分辨率，请转去检查桌面环境的指针加速设置；
   - 如果 resolution 为 0：确认 libinput 的 `evdev_fix_abs_resolution()` 与
     `tp_init_default_resolution()` 会如何猜测尺寸（默认 69x50mm），
     并解释为什么会导致 X/Y 比例失真。

输出：设备信息、各轴范围与分辨率、根因判断、建议的修复方向。
```

## 2. 修复：生成 libinput quirk

```text
上一步确认了：设备 <vendor:product>，ABS 范围 <Rx>x<Ry>，两轴 resolution=0，
libinput 版本 <版本>，桌面 <KDE/GNOME> <Wayland/X11>。

请生成一个 libinput quirk 文件，要求：
1. 用 `MatchUdevType=touchpad`、`MatchBus=ps2`（按实际总线改）、
   `MatchVendor=`、`MatchProduct=`，并加 `MatchDMIModalias=` 限制到本机型；
2. 属性用 `AttrSizeHint=<W>x<H>`（单位 mm）；
3. 先给一个接近真实的初始值（例如 100x55），并说明如何根据实测比例调整；
4. 给出安装路径 `/etc/libinput/local-overrides.quirks`、权限 644、
   重启 KWin / 重登的命令，以及验证命令：
   `sudo libinput quirks list /dev/input/eventN`
   `sudo libinput list-devices | grep -A8 <设备名>`
5. 特别提醒：libinput 的分辨率是整数截断，
   `res_x = floor(Rx/W)`、`res_y = floor(Ry/H)`，改参数要跨过整数台阶。
```

## 3. 调参：根据实测圆比例算下一组参数

```text
用离线画圆页测得的比例（竖向长度 ÷ 横向长度）是 R = <例如 1.07>。
当前 quirk 参数：AttrSizeHint=<W0>x<H0>，坐标范围 <Rx>x<Ry>。
已知 libinput 的分辨率按整数截断：res_x = floor(Rx/W0)，res_y = floor(Ry/H0)。

请：
1. 用公式 目标 res_x/res_y = (当前 res_x/res_y) / R 计算目标比值；
2. 枚举 W、H（W 在合理范围，如 40~200；H 在 20~100），
   找出 floor(Rx/W)/floor(Ry/H) 最接近目标值的整数组合；
3. 说明每组候选的预测 R 和误差；
4. 推荐一组参数（优先让预测 R 最接近 1.00），并给出改完后重启 KWin 的命令；
5. 提醒：形状只由 res_x/res_y 决定；W、H 同比例缩放只改变整体指针速度。
```

## 4. 验证与回归

```text
我按建议把 AttrSizeHint 改成了 <W>x<H> 并重启了 KWin。请给我一份验证清单：
1. `sudo libinput quirks list /dev/input/eventN` 应看到什么；
2. `sudo libinput list-devices` 里 Size 应显示多少（按 floor 后的分辨率反算）；
3. 如何用 `libinput quirks list --verbose` 确认每个 Match 条件都命中；
4. 如果 Size 没变，最可能的原因是什么（整数截断、DMI 不匹配、文件权限、
   未重启会话、udev 属性覆盖）；
5. 回滚步骤。
```

## 5. 通用模板（非 ALPS / 非本机型）

```text
<发行版> + <桌面> + <Wayland/X11>，触控板 <名称>，
现象：<描述>。请帮我：
1. 判断这是"内核分辨率缺失"、"桌面加速设置"还是"硬件问题"；
2. 如果是分辨率缺失，按 libinput quirk（AttrSizeHint）方案修复；
3. 如果内核已报分辨率，检查桌面环境的指针加速/缩放设置；
4. 给我一套可复现的命令和验证步骤，并解释每一步在做什么。
要求：先收集证据再动手；不要修改 /etc/fstab、不要装内核模块、不要重启整机。
```

---

## 附：本次实际使用的系统提示（精简版）

```text
你是一名 Linux 输入设备调试专家。目标：修复触控板 X/Y 轴灵敏度不一致。
约束：
- 只修改 /etc/libinput/local-overrides.quirks 与用户级会话，不碰内核与 fstab；
- 每步先验证再继续；
- 所有命令考虑 sudo 非交互（`echo <pw> | sudo -S ...`），但注意不要把密码
  和文件内容写进同一个 stdin；
- 数值推导要给出公式和整数截断的影响；
- 最终给出可复现的安装、校准、验证、回滚步骤。
```

> 提示：AI 生成的命令请先读懂再执行；尤其是 `sudo`、磁盘、分区相关操作。
