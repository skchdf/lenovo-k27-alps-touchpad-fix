#!/usr/bin/env bash
# 安装 Lenovo ZHAOYANG K27 (ALPS GlidePoint) 的 libinput quirk。
# 用法：./scripts/install.sh
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$here/../quirk/local-overrides.quirks"
dst=/etc/libinput/local-overrides.quirks

if [[ ! -f "$src" ]]; then
    echo "找不到 quirk 文件：$src" >&2
    exit 1
fi

# 1. 安装前先验证 quirk 语法（读文件即可，无需 root）。
#    LIBINPUT_QUIRKS_DIR 指向仓库的 quirk/ 目录，validate 会解析其中所有
#    .quirks 文件，语法错误时以非 0 退出，配合 set -e 直接中止安装。
if command -v libinput >/dev/null 2>&1; then
    echo "--- 语法验证：libinput quirks validate ---"
    LIBINPUT_QUIRKS_DIR="$(dirname "$src")" libinput quirks validate
else
    echo "提示：未找到 libinput 命令，跳过验证（Arch：sudo pacman -S libinput-tools）" >&2
fi

# 2. 安装（幂等）：目标内容一致则跳过复制；已有不同内容先备份，
#    避免覆盖用户 /etc/libinput/local-overrides.quirks 里原有的其他 quirk 段落。
if [[ -f "$dst" ]] && cmp -s "$dst" "$src"; then
    echo "已是最新：$dst（内容一致，跳过复制）"
else
    if [[ -f "$dst" ]]; then
        backup="${dst}.bak.$(date +%Y%m%d-%H%M%S)"
        sudo cp "$dst" "$backup"
        echo "已备份原有文件：$backup"
    fi
    sudo install -Dm644 "$src" "$dst"
    echo "已安装：$dst"
fi

# 3. quirk 在 libinput 创建设备时（设备 add 事件）读取。PS/2 触控板没有
#    "重新插拔"一说，重启机器当然也行，但重启合成器/注销重登更快。
# KDE Plasma Wayland：重启 KWin 让 quirk 生效（不注销也可以）
if systemctl --user restart plasma-kwin_wayland 2>/dev/null; then
    echo "已重启 plasma-kwin_wayland"
else
    echo "未能重启 plasma-kwin_wayland（不在 Plasma Wayland 会话里？），请注销后重新登录"
fi

# 4. 验证
if command -v libinput >/dev/null 2>&1; then
    echo "--- 验证 ---"
    sudo libinput list-devices | grep -A8 -i "GlidePoint" || true
    echo
    echo "期望 Size: 73x37mm；若显示其他值，见 README 的校准章节。"
else
    echo "提示：安装 libinput-tools 后可用 libinput list-devices 验证（Arch：sudo pacman -S libinput-tools）。"
fi
