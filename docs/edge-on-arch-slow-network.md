# 番外：在"烂网络" Arch 上安装 Microsoft Edge（AUR）踩坑记录

> 这是修触控板时顺带发生的事，但教训很通用，尤其是"下载慢 + 内存盘 /tmp"
> 的组合，值得单独记下来。

## 背景

- 系统：Arch Linux，AUR 里 `microsoft-edge-stable` 是唯一来源，官方仓库和
  archlinuxcn 都没有。
- 网络：极不稳定，境外服务器基本不可达；国内镜像时快时慢。
- 已装 `yay`，但 AUR 构建过程本身仍然要访问微软服务器下载 178 MB 的 `.deb`。

## 坑 1：pacman 镜像慢到 1 B/s

`core.db` 从 USTC 拉取时报 `Operation too slow`。实测几个国内镜像：

```bash
for m in mirrors.ustc.edu.cn mirrors.tuna.tsinghua.edu.cn mirrors.aliyun.com; do
  curl -o /dev/null -s -w "$m %{speed_download} B/s\n" --max-time 10 \
    "https://$m/archlinux/core/os/x86_64/core.db"
done
```

TUNA 最快，于是把 `/etc/pacman.d/mirrorlist` 和 `pacman.conf` 里的
archlinuxcn 都指向 TUNA。

### ⚠️ 事故：`echo 密码 | sudo -S tee` 会把密码写进文件

当时想用管道写文件，写成了：

```bash
echo "Server = https://..." | echo 123 | sudo -S tee /etc/pacman.d/mirrorlist
```

结果 `tee` 的 stdin 是第二个 `echo` 的输出，**整个 mirrorlist 被写成了
`123`**，pacman 直接报 "软件仓库没有配置服务器信息"。

教训：

- 密码和文件内容**绝不能共用同一个 stdin**；
- 正确做法是：普通用户先把内容写到临时文件，再
  `echo 密码 | sudo -S cp 临时文件 目标文件`；
- 或者用 `sudo -S bash -c '...'`，但同样要小心管道顺序。

## 坑 2：AUR 的 `pkgver()` 和 `prepare()` 每次构建都会联网

`microsoft-edge-stable` 的 PKGBUILD（示意）：

```bash
pkgver() {
    _url="https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-stable/"
    _deb=$(curl -fsSL -A "Mozilla/5.0" "$_url" | grep -oP 'href="..._amd64.deb"' | ...)
    ...
}

prepare() {
    cd "$srcdir"
    curl -fsSL -A "Mozilla/5.0" -o edge-stable.deb "$_url"
    bsdtar -xf edge-stable.deb
    ...
}
```

问题：

1. `pkgver()` 每次构建都请求微软的目录页；
2. `prepare()` **无条件重新下载** 178 MB 的 deb —— 单线程 curl，而这个
   连接只有 60~130 KB/s，还时不时 `curl: (56) SSL_read: unexpected eof`；
3. 下载中断后 `makepkg` 直接放弃，重跑又从头再来。

## 解决：aria2 多连接 + 让 PKGBUILD 复用本地文件

### 1. 用 aria2 断点续传（16 连接）

```bash
sudo pacman -S aria2   # 从 TUNA 装，很快

cd ~/edge-build
aria2c -c -x16 -s16 -k1M \
  -o edge-stable.deb \
  "https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-stable/microsoft-edge-stable_150.0.4078.96-1_amd64.deb"
```

`-c` 续传、`-x16` 每服务器 16 连接、`-s16` 分 16 段。单线程 curl 卡在
60 KB/s 的文件，多连接很快就拉完了。

### 2. 打补丁：已有 deb 就不再下载

```bash
# 把 deb 放进 makepkg 的 srcdir
mkdir -p src && cp edge-stable.deb src/

# prepare() 里的 curl 改成条件下载
sed -i 's|curl -fsSL -A "Mozilla/5.0" -o edge-stable.deb "$_url"|[ -f edge-stable.deb ] \|\| curl -fsSL -A "Mozilla/5.0" -o edge-stable.deb "$_url"|' PKGBUILD

# pkgver() 也注释掉，避免再访问微软目录页（版本已在文件头写死）
sed -i '/^pkgver()/,/^}/ s/^/#/' PKGBUILD
```

### 3. 在真实磁盘上构建（不要在 /tmp）

第一次 `makepkg` 在 `/tmp/opencode/edge` 里跑，`package()` 阶段报：

```
cp: 复制 '...product_logo_256.png' 时出错: 超出磁盘配额
```

原因：这台机器的 `/tmp` 是 **1.9 GiB 的 tmpfs（内存盘）**，解压后的 Edge
（约 500 MB+）把它撑爆了。Arch 安装时 `/tmp` 默认就是 tmpfs。

```bash
mv /tmp/opencode/edge ~/edge-build   # 搬到真实磁盘（/ 还有 84G）
cd ~/edge-build && makepkg -f
```

### 4. 安装与收尾

```bash
sudo pacman -U microsoft-edge-stable-150.0.4078.96-1-x86_64.pkg.tar.zst
command -v microsoft-edge   # /usr/bin/microsoft-edge
rm -rf ~/edge-build         # 清理 1.5G 构建目录
```

## 经验清单

- [x] 境外大文件：**aria2 多连接 + 断点续传**，别用单线程 curl。
- [x] AUR 构建前先看 `pkgver()`/`prepare()`，能复用本地文件就复用。
- [x] `/tmp` 可能是 tmpfs（内存盘），大构建放到 `$HOME`。
- [x] `sudo -S` 的密码不要和文件内容共用 stdin。
- [x] 改 `/etc` 下的文件优先"先写临时文件再 `sudo cp`"。
- [x] 装完清理构建目录，别把内存盘/根分区塞满。

## 附：常用命令速查

```bash
# 查包归档大小（避免下载小、安装巨大的意外）
pacman -Si microsoft-edge-stable | grep -E "Download|Installed"

# 只看 URL 不下载
pacman -Sp --needed --noconfirm <pkg>

# 下载到 pacman 缓存（可配合 aria2 预取）
sudo pacman -Sw --noconfirm <pkg>
```
