# 更新日志

本文件记录 A_or_B 各版本的用户可见变更。

## 关于「修复」一节

修复按来源分两类列出：

- **来自上一版本** —— 上一个已发布版本中真实存在的缺陷，本版修复。
  用过旧版本的用户可能遇到过。
- **本版内测修复** —— 发布前测试中发现并当场修复，正式版本中不存在这些缺陷，
  仅作开发过程留档。

## [v2.0.0_Alpha] - 2026-10-17

本版是一次架构重写：业务逻辑全部移入 `aorb_core.dll`，关卡数据不再从文件读取，
存档改为「正式存档 + 自动备份」。用户可见的变化集中在存档与日志两处。

### 新增

- **核心库拆分**：剧情、流程与存档移入 `aorb_core.dll`，对外只暴露
  `include\aorb_core.h` 的 C ABI；界面、按键、颜色留在 EXE 一侧。
  新增参考宿主 `src\main_dll_reference.cpp`，用 `scripts\build.bat ref` 构建。
- **跨模块导出约定**：`include\aorb_export.h` 统一定义 `AORB_API` / `AORB_CALL` /
  `AORB_EXTERN_C` 与 `AORB_API_VERSION`（当前为 3）；新增 `aorb_api_version()`，
  宿主启动时先比对版本再调用其余接口。
- **日志通路**：DLL 侧 `aorb_log`（`include\aorb_log.h`）只产生事件、不落盘；
  宿主侧 `host_logger`（`include\host_logger.h`、`src\host_logger.cpp`）是唯一写入者，
  写 `logs\` 下的文件，每行 flush。宿主未注册 sink 时 DLL 照常工作。
  核心库在初始化、场景切换、存档读写与出错点都会发出带 `core` / `save` 标记的事件。
- **日志文件按启动切分**：每次启动新建 `logs\aorb-YYYYMMDD-HHMMSS.log`（年月日-时分秒），
  不再按天追加，历史日志永不覆盖；同一秒重复启动自动加 `-2` / `-3` 后缀。
  文件第一行是横幅，写明程序版本与 core API，例如
  `===== A_or_B v2.0.0_Alpha | core api 3 | started 2026-10-17 19:24:46 =====`。
- **安装程序**：`installer\Setup.cs` + `scripts\package.bat`，产物是单文件
  `dist\A_or_B-2.0.0-setup.exe`（约 430 KB）。向导流程：游戏介绍 → 选择安装目录 →
  进度条 + 逐文件日志 → 完成（可立即启动、可打开游戏介绍）。
  程序本体与「游戏介绍.txt」压成 `payload.zip` 嵌进安装包，不需要 Inno Setup 之类的工具。
  安装后建开始菜单 / 桌面快捷方式，写入「设置 → 应用」的卸载项。
  卸载界面带「同时删除个人文件（存档）」复选框，默认不勾选——存档保留，重装后能接着玩。
  支持 `/extract <目录>` 只解压不安装，便于自测。
- **存档路径自适应**：程序目录可写时用 `data\save.sav`（便携模式）；
  装到 `Program Files` 这种只读目录时自动改用 `%LOCALAPPDATA%\A_or_B\save.sav`。
  也可用环境变量 `AORB_SAVE` 强制指定。
- **冒烟测试**：`tests\smoke_test.cpp` + `scripts\test.bat`，覆盖新游戏、分支推进、
  结局判定、存档读写与清档、越界选项，并校验 DLL 事件确实落进宿主日志。
- **存档改成 `.sav` + 自动备份 `.bak`**：正式存档为 `data\save.sav`，
  玩家“保存并退出”时写入；游戏进行中按间隔自动把进度备份到 `data\save.bak`
  （默认 10 分钟，`aorb_backup_set_interval()` 可调，传 0 关闭）。
  读取时比较两个文件的修改时间，谁新用谁——崩溃后可以用最近的自动备份恢复，
  此时 `clean = false`，宿主提示“上次未正常退出”。主动保存会删掉 `.bak`。
  旧的 `data\save.json` 首次读取时自动迁移。

### 修改

- **关卡数据改为内置**：全部关卡（正文、选项文字、跳转目标、关号覆盖、结局标记）
  从 `data\levels\level.json` 搬进 `src\aorb_core.cpp` 的 C++ 表 `kScenes[]`，
  编译进 DLL，运行时不读任何外部文件。改剧情 = 改代码 + 重新编译。
  `aorb_init()` 因此去掉 `levelPath` 参数，只保留 `savePath`。
- **主入口迁到 DLL**：`src\main.cpp` 不再自己读关卡 JSON、不再自带存档逻辑，
  改为调用 `aorb_*` 接口。随之删除 `src\save.h`、`data\levels\`、`data\schemas\`
  （旧副本仍在 `versions\v1.1.0_Alpha\` 里）。两个入口现在都链接 `aorb_core.lib`。
- **关卡数据检查**：缺失的 `target`、越界选项等错误在 DLL 里统一产生，
  带 `[core]` 标记写进日志，不再只是控制台上一行提示。
- **目录整理**：`console_color.h` 从 `src\` 移入 `include\`，`json.hpp` 移入 `third_party\`；
  `src\` 现在只放 `.cpp` 实现。编译器与 IntelliSense 的包含路径相应改为
  `/I include /I third_party /I src`。
- **头文件集中**：`aorb_core.h`、`aorb_log.h`、`host_logger.h`、`aorb_export.h`
  移入 `include\`，与 `src\` 下的实现分离。
- **构建分两阶段**：`build.bat` 先编 DLL（产出 `bin\aorb_core.dll` 与
  `build\aorb_core.lib`），再编 EXE；两个入口都链接导入库。
- `scripts\run.bat` 会把参数原样转发给 `build.bat`，因此 `run.bat ref` 可直接运行参考宿主。

### 修复

#### 来自上一版本

- **无**（v1.1.0 中不存在下列问题）。

#### 本版内测修复

> 以下问题在本轮收尾过程中发现并当场修复，v1.1.0_Alpha 中不存在，仅作开发过程留档。

- **批处理换行被改坏**：四个 `.bat` 被写成 LF 换行，`cmd` 解析 `goto` 标签失败，
  编译出错时只报“无法找到批处理标签”而不打印真实原因。已统一改回 CRLF。
- **DLL 编译失败**：`src\aorb_log.cpp` 使用了 `AORB_OK` 却没有包含定义它的头文件。
  现改为使用本文件内的 `kOk` 常量，日志模块不再反向依赖核心库头文件。
- **导出宏重复定义**：`aorb_core.h` 与 `aorb_export.h` 各写了一份 `AORB_API` / `AORB_CALL`，
  容易漂移。现统一由 `aorb_export.h` 提供，`aorb_core.h` 只做包含。
- **`aorb_api_version()` 缺失**：文档和冒烟测试都引用了它，但库里从未实现。现已补上。
- **链接器告警**：`/EXP:` 不是当前 MSVC 链接器的有效选项，已移除，导入库由 `/IMPLIB` 正常产出。
- **日志链路两端没接线**：核心库从未发出任何事件，参考宿主也没注册 sink，
  日志模块成了死代码。现在 DLL 侧在关键路径发事件，`main_dll_reference.cpp`
  启动即注册 `host_log::sink_for_dll` 并记录自身事件。
- **API 版本只记录不校验**：`aorb_api_version()` 补上之后，宿主仅打印版本号就继续调用，
  版本不匹配时等于盲飞。现在 `main_dll_reference.cpp` 先比对再初始化，
  不一致时报错退出；冒烟测试也把版本不一致计为失败。
- **`current_file()` 不加锁**：返回内部 `g_path` 的引用，回调线程读取可能与写入竞争。
  现改为返回副本，并用独立的小锁保护（锁序固定为 `g_mutex` → `g_pathMutex`，不会自死锁）。

## [v1.1.0_Alpha] - 2026-10-06

### 新增

- **存档系统**：新增 `src/save.h`（`SaveSession` 类）。每进入一关自动写入 `data/save.json`，
  记录当前关卡、标题、时间与 `clean` 标记；下次启动检测到存档时显示进度与时间，
  询问“继续游戏 / 重新开始”。
- **退出确认**：游戏内按 `ESC` 弹出“保存并退出 / 直接退出（不保存）/ 继续游戏”。
  选择保存时把 `clean` 置为 `true`；选择不保存则删除存档。
- **异常退出识别**：游戏进行中写入的存档 `clean` 为 `false`，下次启动提示
  “检测到上次游戏未正常退出”，据此区分正常退出与强制关闭。
- **关卡索引**：跳转改用 `std::unordered_map<std::string, const json*>` 建立 id → 关卡索引，
  查找由 O(n) 降为 O(1)。
- **关卡内容**：《只有两个按钮》剧情，共 7 关（含 1 个彩蛋关、3 个结局）。

### 修改

- **数据目录独立**：`level.json`、`level.schema.json` 从 `src\` 移至 `data\levels\`、
  `data\schemas\`；`$schema` 引用路径同步改为 `../schemas/level.schema.json`。
- **构建自动化**：`build.bat` 不再硬编码 `vcvars64.bat` 路径，改为按
  `VCVARS64` 环境变量 → `vswhere.exe` → 常见安装目录的顺序自动探测；
  编译后把 `data\` 完整镜像到 `bin\data\`（先删旧副本，删过的文件会同步消失）。
- **版本归档结构**：`versions\` 下所有快照统一为 `bin\` / `src\` 分目录 + 根目录 `README.md`。

### 修复

#### 来自上一版本

- **双击无法运行（v1.0.0 存在）**：`level.json` 位于 `src\`，程序按相对路径读取，
  从 `bin\` 双击启动时工作目录为 `bin\`，找不到关卡文件，报“关卡文件读取失败”后退出。
  本版将数据移至 `data\` 并在构建时镜像到 `bin\data\`，双击 `bin\A_or_B.exe` 可正常运行。

#### 本版内测修复

> 以下问题在发布前测试中发现并当场修复，前一版本 v1.0.0_Alpha 中不存在，仅作开发过程留档。

- **ESC 无响应**：`leveloption` 中只有提示文字、缺少对应的按键判断，
  导致退出键按下后落入选项匹配并提示“无效按键”。现已补上判断。
- **存档指向失效关卡**：存档记录的关卡 id 在 `level.json` 改动后可能已不存在，
  原逻辑会走到“找不到关卡”并清档。现改为退回 `entry` 重新开始，保留存档。
- **批处理编码**：`build.bat` 中的中文注释在 GBK 代码页下乱码，破坏批处理语法，
  导致构建直接失败。注释已全部改为 ASCII。

### 说明

- 存档路径为相对路径 `data/save.json`，工作目录须为项目根目录。
  从 `bin\` 直接启动会在 `bin\data\` 另建一份，并在下次编译时被清除。
- 尚未实现回退、多存档槽与关卡数据的启动期校验。

## [v1.0.0_Alpha] - 2026-10-04

### 新增

- **关卡系统**：新增 `src/level.json` 关卡数据与 `src/level.schema.json`（JSON Schema draft-07）。
  程序启动后从 `entry` 指定的关卡开始，显示关号、标题、正文与选项，按 `option.key` 跳转到
  `option.target`；`option` 为空数组时流程结束。
- **关号覆盖与后缀**：关卡可选写 `fakelevel` 覆盖真实 `level`，并用 `fakelevel2` 追加后缀
  （如 `第1关?`）；结局关（`type: 4`）不显示关号。
- **JSON 解析**：引入 nlohmann/json 单头文件 `src/json.hpp`（v3.11.3，MIT）。
  （v2.0.0 起移到 `third_party/`，见上方「修改」一节。）

### 修改

- 语言标准由 C++17 升至 **C++23**（MSVC 开关为 `/std:c++23preview`）
- 更新日志、README 的版本信息与目录结构

### 说明

- `src/level.json` 目前仍是占位示例，尚未填入实际剧情。
- 关卡跳转每次按 `id` 遍历 `levels` 查找；关卡变多后可改为建立索引。

## [v0.1.1_Alpha] - 2026-10-03

### 修改

- `console_color.h` 的 `WIN32_LEAN_AND_MEAN` / `NOMINMAX` 加上 `#ifndef` 保护；`main.cpp` 调整包含顺序，使宏在 `Windows.h` 之前生效
- `console_color.h` 新增 `clearScreen()` 接口，提供基于 VT 转义序列的清屏能力
- 清屏不再依赖 `system("cls")`，改为直接输出 VT 转义序列

### 说明

- 本版不改动程序行为，只整理内部结构。

## [v0.1.0_Alpha] - 2026-10-03 12:08

### 新增

- 添加 Windows 系统版本提示与确认对话框，目标系统为 Windows 10 1607 或更高版本。
- 添加 UTF-8 控制台输出设置；设置失败时显示错误提示并退出。
- 添加控制台 RGB/HSL 文字颜色接口，并支持恢复默认颜色。
- 添加程序名称、开发者、邮箱、许可证和版本信息显示。
- 添加按键继续的测试流程；游戏内容尚未实现。

### 说明

- 此版本为 Alpha 测试版，可能存在问题。
- 游戏功能计划在后续版本中实现。