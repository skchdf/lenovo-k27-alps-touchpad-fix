#!/usr/bin/env bash
# 安装 Lenovo ZHAOYANG K27 (ALPS GlidePoint) 的 libinput quirk。
# 用法：./scripts/install.sh
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
src="$here/../quirk/local-overrides.quirks"

if [[ ! -f "$src" ]]; then
    echo "找不到 quirk 文件：$src" >&2
    exit 1
fi

sudo install -Dm644 "$src" /etc/libinput/local-overrides.quirks
echo "已安装：/etc/libinput/local-overrides.quirks"

# KDE Plasma Wayland：重启 KWin 让 quirk 生效（不注销也可以）
if systemctl --user restart plasma-kwin_wayland 2>/dev/null; then
    echo "已重启 plasma-kwin_wayland"
else
    echo "请注销后重新登录（或重启你的合成器）"
fi

if command -v libinput >/dev/null 2>&1; then
    echo "--- 验证 ---"
    sudo libinput list-devices | grep -A8 -i "GlidePoint" || true
    echo
    echo "期望 Size: 73x37mm；若显示其他值，见 README 的校准章节。"
else
    echo "提示：安装 libinput-tools 后可用 libinput list-devices 验证。"
fi
