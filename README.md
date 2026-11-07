# PvZ-Portable Apple

经典初代《植物大战僵尸》PC 年度版（GOTY）的社区移植改进分支，面向 iPhone、iPad 和 Apple 芯片 Mac。项目以 [wszqkzqk 的 PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) 为基础，继承了上游的三端支持、基础玩法、资源加载和跨平台关卡存档。这里主要改进触屏操作、游戏菜单和冒险重玩体验。

**[下载中文游戏资源包](https://github.com/Matrixqlc/PvZ-Portable-Apple/releases/download/v0.1.0/PvZ-GOTY-zh-CN-resources.zip)** · [准备游戏数据](#准备游戏数据) · [构建与安装](#构建与安装)

## 这个分支的改动

本分支由 Matrixqlc 在上游源码基础上修改。首个公开基线整理于 2026-11-07。

- **iPhone 和 iPad** 共用移动端代码。改进单指触摸、前后台切换、键盘唤起和昵称输入。移动端选项用原有字体、石板和勾选框做单列排列，主页保留退出花盆和保存退出流程。游戏画面沿用 4:3 比例。
- **Mac** 的游戏菜单加入玩法选项，保留全屏和 3D 加速，并缩短重开按钮文案。Apple 芯片构建配置可以把游戏和依赖一起编译到 macOS 13.3 目标。
- **三端共用**冒险选关、独立重玩存档和逐关星级。未触发救场车是三星，触发一次是二星，两次及以上是一星。只保留最高星级，回到选关时只播放新获得的星星。
- **再战防守**是独立的金色标记。冒险全通后新开一局，通关且未触发救场车即可获得。旧通关和旧的半局存档不补发标记，原有三星记录照常保留。
- **玩法选项**包括自动收阳光、自动收银币／金币／钻石、显示生命和 0.5×、1×、1.5×、2× 四档调速。植物满血时隐藏血条，选项会跨关卡和重启保留。

开发经过见[开发里程碑](docs/MILESTONES.md)。

## 准备游戏数据

游戏需要经典 PC 年度版的 `main.pak` 和 `properties/`。可以任选一份资源，解压后把这两项放在同一目录。

- **中文资源包**可从[发布附件](https://github.com/Matrixqlc/PvZ-Portable-Apple/releases/download/v0.1.0/PvZ-GOTY-zh-CN-resources.zip)下载。这份资源已用于 iPhone、iPad 和 Mac 测试。
- **英文资源**可参考[第三方 1.2.0.1073 发布页](https://github.com/popcap-games/PvZ/releases/tag/1.2.0.1073)。解压后取出 `main.pak` 和 `properties/` 即可。已在 Mac 上验证启动、选关和存档读回。
- 手里已有经典 PC 年度版数据的玩家，也可以直接使用自己的文件。

## 构建与安装

从源码构建需要一台 Mac。先安装 Xcode 并打开一次，让它完成首次配置。首次设置可用 Homebrew 安装 CMake、Ninja 和 Python，再按下面的命令准备 vcpkg 和本项目。已经安装 vcpkg 的读者可以把 `VCPKG_ROOT` 指向已有目录。 [vcpkg 配置指南](https://learn.microsoft.com/vcpkg/get_started/get-started)

```sh
brew install cmake ninja python
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"
git clone https://github.com/Matrixqlc/PvZ-Portable-Apple.git
cd PvZ-Portable-Apple
```

后面的命令都在这个源码目录执行。如果换了终端窗口，重新设置一次 `VCPKG_ROOT`。

### iPhone 和 iPad

移动端包面向 arm64，系统目标为 iOS／iPadOS 16.4。先构建 App 和未签名的 IPA。

```sh
bash ios/build-ios.sh Release
```

脚本生成 `build-ios/pvz-portable-ios.ipa`，可以用自己的 Apple 开发签名签署后侧载。也可以直接用 Xcode 安装。

1. 打开 `build-ios/game/pvz-portable.xcodeproj`，在 Xcode 的设置里登录自己的 Apple 账号。
2. 在 `pvz-portable` 目标的 **Signing & Capabilities** 启用自动签名，选自己的 Team，并填写自己唯一的 Bundle Identifier。
3. 连接并解锁 iPhone 或 iPad，在 Xcode 选中真机后点 **Run**。如果设备提示启用开发者模式，按设备提示操作。 [Apple 真机运行说明](https://developer.apple.com/documentation/xcode/building-and-running-an-app)
4. 打开游戏一次。在系统「文件」App 的「我的 iPhone／iPad → PvZ Portable」里放入解压后的 `main.pak` 和整个 `properties` 文件夹，再重新打开游戏。也可以通过 Finder 的文件共享传入。

### Apple 芯片 Mac

使用 Xcode 命令行工具、CMake、Ninja 和 vcpkg。从仓库根目录运行：

```sh
cmake -S . -B build-mac -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_TRIPLETS="$PWD/CMake/triplets" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-pvz-ventura
cmake --build build-mac --target pvz-portable
```

可执行文件是 `build-mac/pvz-portable`。把 `main.pak` 和 `properties/` 放在它旁边，或用 `-resdir=/path/to/game-data` 指定目录。

## 验证范围

| 平台 | 验证情况 |
| --- | --- |
| iPad | iPad Air M4 已安装并启动；覆盖安装前后，玩家存档和设置逐字节一致。 |
| iPhone | iPhone 16 Pro Max 已安装并进入新用户界面；横屏键盘仍会挡住部分弹窗。 |
| Mac | Apple 芯片 Mac 已启动原生测试包，并用独立存档验证星级、标记和保存规则。macOS 13.3 是编译目标，尚未在该系统上实机验证。 |

「再战防守」已通过 Mac 受控结算检查。移动端的实机通关验证仍待完成。

## 来源与许可

- 上游源码基线为 [PvZ-Portable 98b6593](https://github.com/wszqkzqk/PvZ-Portable/commit/98b659380014c89c3553fbb2839f5e5a84d6df3d)，上游作者为 Zhou Qiankang（wszqkzqk）。上游原版 [README](https://github.com/wszqkzqk/PvZ-Portable/blob/98b659380014c89c3553fbb2839f5e5a84d6df3d/README.md) 保留完整项目说明与致谢。
- 选关的章节组织和植物缩略图组合借鉴 [lspepinho 的 PvZ-NeverGarden](https://github.com/lspepinho/PvZ-NeverGarden/blob/3543de26feefc0cd4868e895222684274ba8a489/src/Lawn/Widget/LevelSelector.cpp)。自动收阳光和调速的实现调研参考 [CypressVillage 的 PvZ-Portable-Mods](https://github.com/CypressVillage/PvZ-Portable-Mods)。
- 项目沿用上游的 LGPL-3.0-or-later 许可声明。仓库附 [LGPL 全文](LICENSE)、[GPL 全文](COPYING)及 [PopCap Games Framework 许可](src/SexyAppFramework/LICENSE)，第三方组件的许可保留在对应目录。原版游戏属于 PopCap Games／Electronic Arts，本项目为独立社区开发。
- 上游版权声明为 Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>。
- The code is provided "as is", **WITHOUT WARRANTY** of any kind.

> This product includes portions of the PopCap Games Framework, © 2005-2009 PopCap Games, Inc. All rights reserved. (http://popcapframework.sourceforge.net/).
