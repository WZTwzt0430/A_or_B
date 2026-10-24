# A_or_B

> 一个用 C++23 编写的 Windows 控制台文字冒险（Text Adventure）程序。

> **仓库维护**
> 本仓库由 **DeepSeek-V4.1-Flash**（DeepSeek Harness 的编码代理）代管：
> 提交、版本号、更新日志、发布与 PR 审核均由它负责。
> 原作者与版权持有人：WZT（2484896217@qq.com）。
> 代管范围仅限工程流程，代码与剧情的最终决定权仍归作者。

- 当前版本：**v2.0.0_Alpha**（2026/10/17）
- 当前状态：早期测试版。业务逻辑全部收进 `aorb_core.dll`，EXE 只做显示、按键与日志
- 目标平台：Windows 10 1607 或更高版本（x64）

---

## 简介

A_or_B 是一个以“选择分支”为核心玩法的文字冒险程序，运行在 Windows 控制台上，
使用 C++23 与 MSVC 构建。

项目目前处于 v2.0.0_Alpha 阶段：程序启动后显示关卡标题与正文，根据玩家按键在关卡之间跳转，
中途按 `ESC` 可以保存退出，游戏进行中会按间隔自动备份，下次启动时询问是否继续。

从这一版开始，**关卡数据、剧情推进、存档读写全部在 `aorb_core.dll` 里**，
运行时不读任何数据文件；`A_or_B.exe` 只是个控制台外壳。

## 当前进度

已实现：

- **系统版本提示**：启动时弹出对话框，说明需要 Windows 10 1607 或更高版本；
  用户选择“取消”则直接退出（返回码 0）
- **UTF-8 控制台**：通过 `SetConsoleOutputCP(CP_UTF8)` 设置输出编码，
  失败时弹窗报错并返回 1
- **控制台真彩色**：`include/console_color.h` 提供 `color(r, g, b)`、`colorHSL(h, s, l)`、
  `resetColor()`、`clearScreen()`，基于 ANSI 24 位真彩色，
  并自动尝试开启 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`
- **许可证提示**：启动时展示 GPL-3.0 版权与免责声明，`show w` / `show c` 查看条款摘要
- **信息展示**：程序名、开发者、邮箱、当前版本与更新时间；
  标题 `A_or_B` 使用 RGB(135, 206, 235) 天蓝色
- **按键继续流程**：提示“按任意键开始游戏”，等待按键后清屏
- **关卡系统**：按 `entry` 定位起始关卡，显示关号 / 标题 / 正文 / 选项，
  按 `option.key` 跳转到 `option.target` 指定的关卡；
  `option` 为空数组时流程结束。关卡跳转使用 `unordered_map` 索引，查找为 O(1)
- **关卡数据内置**：`aorb_core.dll` 用 C++ 表写死全部关卡——
  正文、选项、跳转都在 `src\aorb_core.cpp` 的 `kScenes[]` 里，运行时不读任何文件。
  改剧情就是改代码再编译
- **关号覆盖与后缀**：关卡可选写 `fakelevel` 覆盖真实 `level`，
  并用 `fakelevel2` 追加后缀（如 `第1关?`）；结局关（`type: 4`）不显示关号
- **存档与自动备份**：主动保存写 `data\save.sav`；
  游戏进行中按间隔自动备份到 `data\save.bak`，读取时取较新的那个。
  详见「存档」一节
- **日志**：每次启动在 `logs\` 下新建一个 `aorb-年月日-时分秒.log`，
  头部写明版本与 core API，绝不覆盖历史日志
- **构建自动化**：`build.bat` 自动探测 `vcvars64.bat`（支持 `VCVARS64` 环境变量覆盖）
- **冒烟测试**：`scripts\test.bat` 覆盖关卡推进、存档读写、自动备份与 DLL→宿主日志桥接

尚未实现：

- 关卡内容的完整剧情（现有 7 关为示例，尚未定稿）
- 回退、多存档槽、设置界面、声音、多语言等附加功能
- 关卡表的启动期校验（如 `Choice::target` 指向不存在的关卡）

## 环境要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows 10 1607（Build 14393）或更高，x64 |
| 编译器 | MSVC（当前使用 Visual Studio 18 BuildTools 18.10.3，MSVC 14.51.36231） |
| 语言标准 | C++23（MSVC 下开关为 `/std:c++23preview`），源文件为 UTF-8（`/utf-8`），`/EHsc` |
| Windows SDK | 10.0.26100.0 |
| 终端 | 支持 VT 转义序列的终端（Windows Terminal 或新版 conhost），否则标题颜色可能不显示 |
| 依赖库 | 链接 `user32.lib`（对话框）、`conio.h`（`_getch`）；DLL 内部用 `third_party/json.hpp`（nlohmann/json，MIT）写存档 |
| 运行期模块 | `A_or_B.exe` + 同目录的 `aorb_core.dll`（构建时一起产出） |

> 启动时的版本对话框是免责提示：在不受支持的系统上运行后产生的问题，开发者不受理。

## 目录结构

```
A_or_B/
├─ include/                  # 公开头文件（DLL、EXE、测试共用）
│  ├─ aorb_export.h          # 导出宏 / 调用约定 / API 版本号
│  ├─ aorb_core.h            # 核心库 C ABI：关卡、流程、存档
│  ├─ aorb_log.h             # DLL 侧日志回传接口（不落盘）
│  ├─ host_logger.h          # 宿主日志器接口（唯一写文件者）
│  └─ console_color.h        # 控制台真彩色接口（RGB / HSL / 清屏，MIT）
├─ third_party/
│  └─ json.hpp               # nlohmann/json 单头文件（MIT），DLL 内部写存档用
├─ scripts/
│  ├─ build.bat              # 构建：core DLL + EXE
│  ├─ run.bat                # 先构建再运行，透传程序退出码
│  ├─ test.bat               # 构建 + 运行冒烟测试
│  ├─ package.bat            # 打包安装程序
│  ├─ make_payload.ps1       # 生成 payload.zip（被 package.bat 调用）
│  ├─ pack_source.ps1        # 打源码包（dist\A_or_B-src-*.zip）
│  └─ _toolchain.bat         # 探测 vcvars64.bat（被上面几个脚本调用）
├─ installer/
│  ├─ Setup.cs               # 安装程序源码（.NET WinForms，单文件）
│  ├─ setup.manifest         # 要求管理员权限的清单
│  └─ Readme.txt             # 安装后附带的「游戏介绍.txt」
├─ src/                      # 只放实现（*.cpp）
│  ├─ main.cpp               # 正式宿主：许可证提示、信息栏、显示与按键
│  ├─ main_dll_reference.cpp # 精简参考宿主（同功能的最小实现）
│  ├─ aorb_core.cpp          # 核心库实现：★ 关卡表 kScenes[] 就在这里
│  ├─ aorb_log.cpp           # 日志回传实现（存 sink、过滤、回调）
│  └─ host_logger.cpp        # 宿主日志器实现（写 logs\aorb-年月日-时分秒.log）
├─ tests/
│  └─ smoke_test.cpp         # 冒烟测试：走通关卡、存档、备份与日志
├─ data/                     # 运行期数据：save.sav / save.bak（首次运行时生成）
├─ logs/                     # 运行期日志，每次启动一个文件
├─ versions/                 # 各版本快照（见「版本备份」）
│  ├─ v0.1.0_Alpha/
│  ├─ v0.1.1_Alpha/
│  ├─ v1.0.0_Alpha/
│  ├─ v1.1.0_Alpha/
│  └─ v2.0.0_Alpha/
│     └─ VERSION.txt         # 版本说明 + 重建步骤
├─ bin/                      # 构建产物（A_or_B.exe + aorb_core.dll）
├─ build/                    # 中间产物（*.obj / *.pdb / aorb_core.lib / payload.zip）
├─ dist/                     # 打包产物（A_or_B-x.y.z-setup.exe）
├─ .vscode/                  # 编辑器构建 / 调试 / IntelliSense 配置
├─ make.bat                  # 一键编译（双击即可）
├─ CHANGELOG.md              # 更新日志（面向用户的版本变更）
├─ LICENSE.txt               # GPL-3.0 许可证全文
└─ README.md                 # 本文件
```

## 内部结构：core DLL + 宿主 EXE

职责边界：

- **`aorb_core.dll`**：内置关卡表、剧情推进、存档与自动备份、日志事件产生。
  对外只有 `include\aorb_core.h` 里的 C ABI，不涉及界面、颜色、按键。
- **`A_or_B.exe`**：控制台显示、按键读取、退出确认、许可证提示、日志落盘。
  即使 DLL 初始化失败，宿主自身的日志仍然能写。
- **`aorb_log` 与 `host_logger`**：DLL 只通过 sink 回调向外抛事件，自己不碰磁盘；
  写文件的是宿主，保证同一时刻只有一个写入者。
- **API 版本**：`aorb_api_version()` 返回编译期 `AORB_API_VERSION`（当前为 3），
  宿主启动时先比对，不一致就退出。跨模块统一 `__cdecl` 调用约定。
- **初始化**：`aorb_init(savePath)` 只建关卡索引、记下存档路径——关卡数据在编译期
  就已经在 DLL 里了，没有文件可加载，也就没有“读关卡失败”这种启动错误。

两个入口，功能等价：

| 入口 | 说明 |
|------|------|
| `src\main.cpp`（默认） | 完整宿主：版本对话框、GPL 提示、信息栏 |
| `src\main_dll_reference.cpp`（`build.bat ref`） | 精简版，只留显示与按键，适合当模板读 |


## 关卡表（编译进 DLL）

`aorb_core.dll` 里有一张 **C++ 硬编码的关卡表**，剧情正文、选项文字、跳转目标全部是代码，
运行时不读任何外部文件。表定义在 `src\aorb_core.cpp` 的「关卡表」区：

```cpp
struct Scene {
    const char* id;
    int         type;         // 1 教学 / 2 普通 / 3 彩蛋 / 4 结局
    int         level;        // 真实关号，结局关填 0
    int         fakeLevel;    // 覆盖关号，0 = 不覆盖
    const char* fakeLevel2;   // 关号后缀，nullptr = 无
    const char* title;
    const char* const* content;
    int         contentCount;
    const Choice* choices;    // 结局关填 nullptr
    int         choiceCount;
};
```

- **加一关**：写一个正文数组 + 一个选项数组，往 `kScenes[]` 里加一行
- **改剧情**：直接改字符串，不用碰任何其它文件
- **改跳转**：改 `Choice::target`
- **起始关卡**：`kEntryId`（当前是 `elev_1`）

`data\levels\level.json` 与 `data\schemas\` 已在 v2.0.0 移除：
关卡数据全部在 DLL 里，`main.cpp` 也不再读 JSON。旧副本仍留在
`versions\v1.1.0_Alpha\data\` 里备查。

## 存档

用两个文件，各管一件事：

| 文件 | 什么时候写 | `clean` |
|------|-----------|---------|
| `data\save.sav` | 玩家主动“保存并退出” | `true` |
| `data\save.bak` | 游戏进行中，按间隔自动备份（默认 10 分钟，可调） | `false` |

规则：

- **自动备份**：进关卡时调用 `aorb_save_write(0)`，DLL 内部按间隔节流，
  到点才写 `.bak`；没到点直接跳过。
- **玩家保存**：`aorb_save_write(1)` 写 `.sav`，并**删掉** `.bak`——
  正式存档是最新的，备份没有意义了。
- **读取**：比较两个文件的修改时间，**谁新用谁**。崩溃前最后一次自动备份
  可能比上次手动保存还新，这时用 `.bak` 恢复，而它的 `clean = false`，
  宿主据此提示“检测到上次游戏未正常退出”。
- **间隔可调**：`aorb_backup_set_interval(seconds)`，默认 `600`，传 `0` 关闭自动备份。
  10 分钟只是默认值，不是写死的。
- **旧格式迁移**：`data\save.json`（v1.1.0 的格式）仍然兼容，
  首次读取时自动迁移成 `.sav`。

文件内容仍是 JSON，字段不变：

| 字段 | 类型 | 说明 |
|------|------|------|
| `level` | string | 上次停留的关卡 id |
| `title` | string | 该关标题，供恢复面板显示 |
| `time` | string | 存档时间，形如 `2026-10-06 18:01:23` |
| `clean` | boolean | `true` = 玩家主动保存；`false` = 自动备份 |

文件损坏或字段缺失时按“无存档”处理，不会导致程序异常。

## 构建

最省事的方式：**双击根目录的 `make.bat`**。它会自动找 `vcvars64.bat`、编译两阶段，
结束时窗口停住让你看结果。命令行下也可以直接跑，加 `/nopause` 不等待按键：

```bat
make.bat
make.bat /nopause
make.bat ref        :: 用精简参考宿主
```

也可以绕过 `make.bat` 直接调用脚本：

```bat
scripts\build.bat        :: 默认：编译 core DLL，EXE 用 src\main.cpp
scripts\build.bat ref    :: 编译 core DLL，EXE 用 src\main_dll_reference.cpp
scripts\test.bat         :: 构建后跑冒烟测试
```

构建分两个阶段：

1. `src\aorb_core.cpp` + `src\aorb_log.cpp` 编成 `bin\aorb_core.dll`，
   导入库落在 `build\aorb_core.lib`；
2. `src\main.cpp`（或 `main_dll_reference.cpp`）+ `src\host_logger.cpp` 编成
   `bin\A_or_B.exe`。两个入口都链接 `build\aorb_core.lib`。

脚本按以下优先级定位 `vcvars64.bat`：

1. `VCVARS64` 环境变量（显式指定，最高优先级）
2. `vswhere.exe`（Visual Studio 官方定位工具，只挑装了 C++ 工具集的实例）
3. `Program Files` / `Program Files (x86)` 下的常见安装路径

编译前会打印实际使用的路径，例如：

```
[toolchain] D:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
```

`data\` 里如果有文件，会整份镜像到 `bin\data\`（先删旧副本再复制）。
成功时输出 `Build succeeded: <项目路径>\bin\A_or_B.exe` 和 `bin\aorb_core.dll`；
失败返回 1 并打印对应的错误原因。

方式二：VS Code 中按 `Ctrl+Shift+B`，选择 `Build C++ Project`。

等价的编译命令（默认模式）：

```bat
cl /nologo /EHsc /std:c++23preview /utf-8 /Fo"build\main.obj" /Fe"bin\A_or_B.exe" "src\main.cpp" "src\host_logger.cpp" /link user32.lib "build\aorb_core.lib"
```

换机器时如果自动探测失败，可以手动指定：

```bat
set VCVARS64=D:\path\to\BuildTools\VC\Auxiliary\Build\vcvars64.bat
scripts\build.bat
```

## 测试

```bat
scripts\test.bat
```

冒烟测试（`tests\smoke_test.cpp`）连的是真实的 `aorb_core.lib`，覆盖：

1. 开始新游戏、读取关卡信息
2. 按 A → B 走到彩蛋关
3. 选择彩蛋选项走到结局
4. 自动备份写 `.bak`、不碰 `.sav`
5. 从存档继续
6. “保存并退出”写 `.sav` 并删掉 `.bak`
7. 清档后确认无存档
8. 越界选项返回 `NULL` 且带错误描述
9. 自动备份的间隔节流（间隔内不重复写）
10. 主动保存让自动备份作废
11. 崩溃恢复：只剩 `.bak` 时能读出来且 `clean = 0`
12. 旧格式 `save.json` 迁移成 `.sav`
13. 日志桥接：DLL 产生的 `[core]` / `[save]` 事件确实落进了宿主的日志文件

全部通过时最后输出 `ALL OK`，退出码 0。

## 打包安装程序

```bat
scripts\build.bat        :: 先出 bin\A_or_B.exe + bin\aorb_core.dll
scripts\package.bat      :: 再打包成安装程序
```

产物：`dist\A_or_B-2.0.0-setup.exe`，单文件约 430 KB。

打包做了什么：

1. `bin\A_or_B.exe`、`bin\aorb_core.dll`、`installer\Readme.txt`（改名为「游戏介绍.txt」）
   压成 `build\payload.zip`；
2. 用系统自带的 `csc.exe` 编译 `installer\Setup.cs`（.NET Framework 4.x 的 WinForms），
   把 `payload.zip` 作为资源嵌进去；
3. 不需要安装 Inno Setup / NSIS 之类的工具。

安装程序的流程是标准向导：

| 页面 | 内容 |
|------|------|
| 介绍 | 游戏简介、版本、开发者；可「下一步」或「取消」 |
| 选择目录 | 默认 `Program Files\A_or_B`，可浏览，可「上一步」 |
| 安装 | 进度条 + 逐文件详细日志 |
| 完成 | 勾选「立即启动」「打开游戏介绍」 |

安装后：开始菜单与桌面快捷方式、「设置 → 应用」里可见的卸载项。
卸载界面有个复选框「同时删除个人文件（存档）」，**默认不勾选**，即默认保留存档（`%LOCALAPPDATA%\A_or_B`）；卸载完成后安装目录（含 uninstall.exe 自身）会被整个删掉。

自测（不弹界面、只解压）：

```bat
dist\A_or_B-2.0.0-setup.exe /extract build\payload_test
```

> 安装包目前**没有代码签名**，从网上下载后运行会触发 SmartScreen 的
> “Windows 已保护你的电脑”。点「更多信息 → 仍要运行」可以继续；
> 要彻底消除这个提示，必须购买代码签名证书，见「分发与签名」。

## 打源码包

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\pack_source.ps1
```

产物：`dist\A_or_B-src-v2.0.0.zip`，约 205 KB。

里面是「解压就能编译」的完整源码：`make.bat`、`src\`、`include\`、`third_party\json.hpp`、
`scripts\`、`installer\`、`tests\` 和文档。不含 `bin\`、`build\`、`dist\`、`data\`、`logs\`、`versions\`。

解压后双击 `make.bat` 即可得到 `bin\A_or_B.exe`，已经实测过从干净目录构建。

## 运行

```bat
scripts\run.bat        :: 构建并运行默认入口
scripts\run.bat ref    :: 构建并运行 DLL 参考宿主
```

`run.bat` 会先执行构建（参数原样转给 `build.bat`），构建失败则不会启动程序，
并把程序的退出码原样返回。

也可以构建后直接运行 `bin\A_or_B.exe`，但**不推荐**：程序按相对路径写存档，
工作目录必须是项目根目录；从 `bin\` 启动会把存档另建一份到 `bin\data\`，
且该副本会在下次编译时被清除。无论哪种方式，`bin\aorb_core.dll` 都必须和 EXE 在同一目录。

想快速验证自动备份，可以加参数缩短间隔：

```bat
bin\A_or_B.exe --backup-interval=5
```

## 分发与签名

程序和安装包目前都**没有代码签名**，从网上下载后首次运行会看到
“Windows 已保护你的电脑”。三条路可选：

| 方式 | 效果 | 成本 |
|------|------|------|
| 不签名 | 用户点两次「更多信息 → 仍要运行」即可，但每次下载都会被拦 | 0 |
| 自签名证书 | 只对导入过该证书的机器有效，陌生人那照样拦 | 0 |
| 商业代码签名证书（OV/EV） | 陌生机器也不拦；EV 即时信誉，OV 需要累积下载量 | 几百美元/年 |

自签名（自己或小圈子用）：

```powershell
# 1. 生成代码签名证书
New-SelfSignedCertificate -Type CodeSigningCert `
  -Subject "CN=WZT, O=A_or_B, C=CN" `
  -FriendlyName "A_or_B dev signing" `
  -CertStoreLocation "Cert:\CurrentUser\My" `
  -NotAfter (Get-Date).AddYears(5)

# 2. 导出
$cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
        Where-Object { $_.Subject -like "*A_or_B*" }
Export-PfxCertificate -Cert $cert -FilePath aorb-dev.pfx `
  -Password (ConvertTo-SecureString "换成你的密码" -Force -AsPlainText)
Export-Certificate   -Cert $cert -FilePath aorb-dev.cer

# 3. 签名（EXE 和 DLL 都要签；安装包也签）
signtool sign /f aorb-dev.pfx /p 换成你的密码 /fd SHA256 `
  /tr http://timestamp.digicert.com /td SHA256 `
  bin\A_or_B.exe bin\aorb_core.dll dist\A_or_B-2.0.0-setup.exe

# 4. 别人的机器导入信任（管理员）
Import-Certificate -FilePath aorb-dev.cer -CertStoreLocation Cert:\LocalMachine\Root
```

注意：**每次重新编译都要重新签名**，签名绑定文件内容。

另一条免费路线是 Microsoft Store（MSIX）：商店代签，用户从商店安装不触发 SmartScreen，
代价是要做 MSIX 打包和审核。

## 程序退出码：

| 退出码 | 含义 |
|--------|------|
| 0 | 正常运行结束，或用户在版本提示中选择“取消” |
| 1 | 设置控制台 UTF-8 编码失败，或 DLL 版本不匹配 |

## 日志

- 目录：`logs\`，**每次启动新建一个文件**：`aorb-YYYYMMDD-HHMMSS.log`（年月日-时分秒），
  从不覆盖历史日志。同一秒内重复启动会自动加 `-2` / `-3` 后缀。
- 文件第一行是横幅，写明版本与 core API，例如：

  ```
  ===== A_or_B v2.0.0_Alpha | core api 3 | started 2026-10-17 19:24:46 | log logs\aorb-20261017-192446.log =====
  ```

- 写入者只有宿主（`src\host_logger.cpp`）。DLL 通过 `aorb_log_set_sink()`
  注册的回调把事件转进来，自己绝不写文件。
- DLL 加载失败时宿主日志照样能写，因此启动异常也能查到记录。
- 级别：`DEBUG` / `INFO` / `WARN` / `ERROR`，默认 `INFO`。
  目录建不出来时日志静默停摆，不影响游戏运行。

## 程序启动流程（`src\main.cpp`）

1. 初始化宿主日志（`logs\`），横幅记录版本与 core API
2. 调用 `SetConsoleOutputCP(CP_UTF8)`；失败则弹窗报错并返回 1
3. 弹出 Windows 版本确认对话框（需要 Windows 10 1607+）；用户选择“取消”则返回 0
4. 展示 GPL-3.0 版权与免责声明，可输入 `show w` / `show c`，回车继续
5. 以天蓝色输出标题 `A_or_B`，随后输出开发者、邮箱、版本等信息
6. 输出提示“按任意键开始游戏”，用 `_getch()` 等待按键
7. 注册 DLL 日志 sink，比对 `aorb_api_version()`；不一致返回 1
8. `aorb_init("data/save.sav")`——只建关卡索引，没有文件可读
9. 有存档则询问“继续游戏 / 重新开始”，否则从头开始
10. 循环：显示关号 / 标题 / 正文 / 选项 → 自动备份 → 等待按键 → `aorb_choose()`
11. `ESC` 弹出退出确认（保存并退出 / 直接退出 / 继续游戏）；走到结局清档退出

`src\main_dll_reference.cpp` 的流程与之等价，只是省掉了版本对话框与许可证提示。

## 版本备份

> **这是过渡做法。** 项目还没有版本控制，只能靠手工复制目录留档；
> 将来接入 git 之后，`versions\` 会被提交历史取代，这里不再是唯一的历史来源。

约定：每个需要留档的版本在 `versions\` 下建立独立目录，目录名形如
`v<版本号>_<阶段>`，快照必须**能独立重建**：

| 收录 | 不收录 |
|------|--------|
| `bin\`：已构建的 `A_or_B.exe` + `aorb_core.dll` | `build\` 中间产物、`.vscode\` 编辑器配置 |
| `src\`、`include\`：全部 `.cpp` 与 `.h` | `third_party\json.hpp`（第三方，重建时按需复制） |
| `scripts\`、`installer\`、`tests\` | `data\`、`logs\` 运行期数据 |
| `dist\`：安装程序成品 | |
| `README.md`、`CHANGELOG.md`、`LICENSE.txt`、`VERSION.txt` | |

每个快照根目录有一份 `VERSION.txt`，写明版本、日期、收录范围与重建步骤。

现有快照：`v0.1.0_Alpha`、`v0.1.1_Alpha`、`v1.0.0_Alpha`、`v1.1.0_Alpha`、`v2.0.0_Alpha`。

## 版本历史

见 `CHANGELOG.md`。当前最新版本为 `v2.0.0_Alpha`（2026-10-17）：
业务逻辑全部移入 DLL、关卡数据内置、存档改为 `.sav` + 自动备份。

## 文档

- `CHANGELOG.md`：各版本的用户可见变更
- `LICENSE.txt`：GPL-3.0 许可证全文

## 待办

- 补完 7 关的正式剧情（现在是示例稿）
- 关卡表的启动期校验：`Choice::target` 指向不存在的 id 时直接报错，而不是运行时才发现
- 不被 Microsoft Defender SmartScreen 拦截

## 许可证

本程序整体遵循 **GPL-3.0** 协议开源，许可证全文见 `LICENSE.txt`。

`include/console_color.h` 单独以 **MIT** 许可提供（宽松许可，可被单独取用，也可并入 GPL 项目）；
`third_party/json.hpp` 为第三方 nlohmann/json，同样为 MIT 许可，版权归其原作者所有。

## 作者

- 开发者：WZT
- 协作：DeepSeek Harness
- 邮箱：2484896217@qq.com
