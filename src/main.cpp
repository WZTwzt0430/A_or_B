/*
    A_or_B is a Windows console program and experimental game project.
    Copyright (C) 2026 WZT

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://gnu.net.cn/licenses/>.

    Developer e-mail:2484896217@qq.com
*/
/*
    A_or_B 是一个 Windows 控制台程序和实验性游戏项目。
    版权所有 (C) 2026 WZT  

    本程序是自由软件：您可以在遵守自由软件基金会发布的 GNU 通用公共许可证的条款下重新分发和/或修改本程序，版本为 3，或者（根据您的选择）任何更高版本。

    本程序的发布旨在希望它能有所帮助，但不提供任何担保；甚至没有适销性或适合特定用途的隐含担保。有关更多详细信息，请参阅 GNU 通用公共许可证。

    您应该已经收到随本程序一起提供的 GNU 通用公共许可证的副本。如果没有，请参阅 <https://gnu.net.cn/licenses/>。

    开发者邮箱：2484896217@qq.com
*/
/*
    宿主程序。2026-10-17 起业务逻辑全部移入 aorb_core.dll：
      关卡数据、剧情推进、存档读写都在 DLL 里，
      本文件只负责 显示、按键、许可证提示、日志落盘。
*/
//使用C++23标准编译
//我并没有打算做跨平台移植，所以只在 Windows 下测试过。其他平台请自行修改代码。
#include <iostream>
#include <string>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <conio.h>
#include <Windows.h>
#include "aorb_core.h"
#include "aorb_log.h"
#include "host_logger.h"
#include "console_color.h"
#pragma comment(lib, "user32.lib")
#define NOMINMAX
using namespace console_color;

namespace {

constexpr const char* kAppVersion = "v2.0.0_Alpha";
constexpr const char* kAppBuild   = "2026/10/17 19:40";
constexpr int kEsc = 27;

void printWarrantyNotice() {
    std::cout
        << "\nGPL-3.0 第15节：无担保\n"
        << "本程序按“现状”提供，不提供任何明示或默示担保，包括适销性或适合特定用途的担保。\n"
        << "使用和传播本程序的风险由使用者承担；除法律要求或书面承诺外，相关权利人不承担因使用本程序造成的损失。\n"
        << "以上为条款要点摘要，完整内容请查阅 GPL-3.0：\n"
        << "https://gnu.net.cn/licenses/\n\n";
}

void printLicenseConditions() {
    std::cout
        << "\nGPL-3.0 许可条件摘要\n"
        << "您可以在 GPL-3.0 许可范围内运行、修改和再分发本程序。\n"
        << "再分发时应保留版权与许可证声明，并向接收者提供 GPL-3.0 许可证副本。\n"
        << "分发修改版本时，应标明修改；分发可执行版本时，还须按 GPL-3.0 的要求提供对应源代码或有效的源代码获取方式。\n"
        << "完整条件取决于具体分发方式，请查阅 GPL-3.0 全文：\n"
        << "https://gnu.net.cn/licenses/\n\n";
}

void showLicensePrompt() {
    std::cout
        << "A_or_B 版权所有 (C) 2026 WZT\n"
        << "本程序不提供任何担保；输入 show w 查看无担保说明。\n"
        << "本程序是自由软件，欢迎您在遵守相关条件的前提下再分发；输入 show c 查看 GPL-3.0 许可条件。\n";

    std::string command;
    while (true) {
        std::cout << "输入 show w、show c，或直接按回车继续\n>";
        if (!std::getline(std::cin, command) || command.empty()) {
            return;
        }

        if (command == "show w") {
            printWarrantyNotice();
        } else if (command == "show c") {
            printLicenseConditions();
        } else {
            std::cout << "未知命令。请输入 show w、show c，或直接按回车继续。\n";
        }
    }
}

// 显示当前场景的标题行与正文
void showScene() {
    color(135, 206, 235);
    std::cout << aorb_scene_label() << "   " << aorb_scene_title() << std::endl;
    resetColor();

    std::cout << std::string(65, '-') << std::endl;
    std::cout << aorb_scene_content() << std::endl;
    std::cout << std::string(65, '-') << std::endl;
}

// 显示选项。返回选项数量。
int showChoices() {
    const int n = aorb_choice_count();
    for (int i = 0; i < n; ++i) {
        std::cout << "  " << aorb_choice_key(i)
                  << ". " << aorb_choice_text(i) << std::endl;
    }
    if (n > 0) {
        std::cout << "  ESC. 退出游戏" << std::endl;
    }
    return n;
}

// 等玩家按键。返回选中的索引，或 -1 表示按了 ESC。
int waitChoice(int n) {
    while (true) {
        const int raw = _getch();
        if (raw == kEsc) return -1;

        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        for (int i = 0; i < n; ++i) {
            const char* k = aorb_choice_key(i);
            if (k != nullptr && k[0] == c) return i;
        }
        std::cout << "无效按键，请重新选择\n";
    }
}

// 启动菜单：有存档时询问。返回 true = 继续存档。
bool askContinue() {
    AorbSaveData d{};
    if (aorb_save_read(&d) != AORB_OK || !d.valid) return false;

    std::cout << "\n";
    if (d.clean) {
        std::cout << "检测到存档\n";
    } else {
        std::cout << "检测到上次游戏未正常退出\n";
    }
    std::cout << "  进度：" << (d.title[0] ? d.title : d.level)
              << "（" << d.level << "）\n"
              << "  时间：" << d.time << "\n\n"
              << "  1. 继续游戏\n"
              << "  2. 重新开始\n";

    while (true) {
        std::cout << "请选择 (1/2)> ";
        std::cout.flush();
        const int c = _getch();
        if (c == '1') { std::cout << "1\n"; clearScreen(); return true; }
        if (c == '2') { std::cout << "2\n"; aorb_save_clear(); clearScreen(); return false; }
    }
}

// 退出确认。返回 true = 真的退出。
bool askQuit() {
    std::cout << "\n确定要退出游戏吗？\n"
              << "  1. 保存并退出\n"
              << "  2. 直接退出（不保存）\n"
              << "  3. 继续游戏\n";
    while (true) {
        std::cout << "请选择 (1/2/3)> ";
        std::cout.flush();
        const int c = _getch();
        if (c == '1') {
            std::cout << "1\n";
            aorb_save_write(1);                    // 正式存档，clean = 1
            std::cout << "已保存，下次可从这一关继续\n";
            return true;
        }
        if (c == '2') {
            std::cout << "2\n";
            aorb_save_clear();
            std::cout << "未保存\n";
            return true;
        }
        if (c == '3') { std::cout << "3\n"; clearScreen(); return false; }
    }
}

}  // namespace

// 存档位置：
//   1. 环境变量 AORB_SAVE 指定的路径（最高优先级）
//   2. 当前目录可写 -> data\save.sav（便携模式，直接跑在项目目录里）
//   3. 否则 %LOCALAPPDATA%\A_or_B\save.sav
//      —— 装到 Program Files 时程序目录是只读的，必须换到用户目录，
//         否则存档写不进去。
std::string resolveSavePath() {
    if (const char* env = std::getenv("AORB_SAVE")) {
        if (env[0] != '\0') return env;
    }

    std::error_code ec;
    std::filesystem::create_directories("data", ec);
    if (!ec) {
        const std::string probe = "data\\.aorb_write_probe";
        std::ofstream t(probe, std::ios::out | std::ios::trunc);
        if (t) {
            t.close();
            std::filesystem::remove(probe, ec);
            return "data/save.sav";
        }
    }

    if (const char* local = std::getenv("LOCALAPPDATA")) {
        if (local[0] != '\0') {
            const std::string dir = std::string(local) + "\\A_or_B";
            std::filesystem::create_directories(dir, ec);
            return dir + "\\save.sav";
        }
    }
    return "data/save.sav";   // 兜底
}

int main(int argc, char** argv) {
    // 日志先于一切初始化：即使后面 DLL 初始化失败，也要留下记录。
    // 横幅写明版本与 core API，方便事后辨认这份日志属于哪个构建。
    const int api = aorb_api_version();
    host_log::init("logs", std::string(kAppVersion) + " | core api " + std::to_string(api));
    host_log::set_level(host_log::LevelInfo);
    host_log::info("host", "A_or_B %s starting", kAppVersion);

    if (GetConsoleWindow() != nullptr && !SetConsoleOutputCP(CP_UTF8)) {
        host_log::error("host", "SetConsoleOutputCP(CP_UTF8) failed");
        MessageBoxW(nullptr, L"设置控制台输出编码为UTF-8失败，请检查系统设置或权限。",
                    L"A_or_B程序错误", MB_OK | MB_ICONERROR);
        host_log::shutdown();
        return 1;
    }

    const int result = MessageBoxW(
        nullptr,
        L"此程序需要Windows10 1607或更高版本才能正常运行。\n请检查自己的系统版本,确认是否在支持的范围内。\n如果在不受支持的版本上运行后出了问题，开发者将不受理问题反馈。",
        L"A_or_B程序警告", MB_OKCANCEL | MB_ICONWARNING);
    if (result == IDCANCEL) {
        host_log::info("host", "user cancelled at version prompt");
        host_log::shutdown();
        return 0;
    }

    color(240, 128, 128);
    showLicensePrompt();
    resetColor();
    std::cout << "\033[2J\033[1;1H";

    color(135, 206, 235);
    std::cout << "A_or_B" << std::endl;
    resetColor();
    std::cout << "开发者: WZT,Deepseek Harness\n"
              << "邮箱: 2484896217@qq.com\n"
              << "当前程序版本: " << kAppVersion << "\n"
              << "此版本更新时间:" << kAppBuild << "\n"
              << std::string(65, '-') << "\n"
              << "本程序为A_or_B程序的测试版本，可能存在一些问题，请谨慎使用\n"
              << "如果在使用过程中遇到问题，请联系开发者反馈\n"
              << std::string(65, '-') << "\n"
              << "按任意键开始游戏";
    _getch();
    std::cout << "\033[2J\033[1;1H";

    // DLL 内部的事件从这里汇入同一个日志文件
    aorb_log_set_sink(host_log::sink_for_dll, nullptr);

    // 比对 API 版本：不匹配就停，别拿不兼容的 DLL 继续跑
    if (api != AORB_API_VERSION) {
        std::cout << "核心库版本不匹配：DLL 为 " << api
                  << "，本程序需要 " << AORB_API_VERSION << "\n";
        host_log::error("host", "core api version mismatch: dll=%d host=%d",
                        api, AORB_API_VERSION);
        host_log::shutdown();
        return 1;
    }

    // 允许命令行覆盖自动备份间隔，方便测试
    {
        const std::string k = "--backup-interval=";
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if (a.rfind(k, 0) == 0) {
                const int sec = std::atoi(a.c_str() + k.size());
                aorb_backup_set_interval(sec);
                host_log::info("host", "backup interval from cli = %ds", sec);
            }
        }
    }

    const std::string savePath = resolveSavePath();
    host_log::info("host", "save path = %s", savePath.c_str());

    if (aorb_init(savePath.c_str()) != AORB_OK) {
        std::cout << "核心库初始化失败：" << aorb_last_error() << "\n";
        host_log::error("host", "core init failed: %s", aorb_last_error());
        host_log::shutdown();
        return 1;
    }

    // ---- 起始场景 ----
    bool resumed = false;
    if (askContinue()) {
        resumed = (aorb_continue_game() != nullptr);
        if (!resumed) {
            std::cout << "存档指向的场景已不存在，从头开始\n";
        }
    }
    if (!resumed) aorb_new_game();

    // ---- 主循环 ----
    while (true) {
        clearScreen();
        showScene();

        const int n = showChoices();

        if (n == 0) {                              // 结局
            host_log::info("host", "reached ending at %s", aorb_current_scene());
            aorb_save_clear();
            std::cout << "\n（结束，按任意键退出）\n";
            _getch();
            break;
        }

        aorb_save_write(0);                        // 进场景就自动备份（按间隔节流）

        const int pick = waitChoice(n);
        if (pick < 0) {                            // ESC
            if (askQuit()) {
                host_log::info("host", "player quit at %s", aorb_current_scene());
                break;
            }
            continue;
        }

        if (aorb_choose(pick) == nullptr) {
            host_log::error("host", "advance failed: %s", aorb_last_error());
            std::cout << "推进失败：" << aorb_last_error() << "\n";
            break;
        }

        Sleep(800);                                // 跳转前留一点余韵
    }

    host_log::info("host", "host exiting");
    aorb_shutdown();
    host_log::shutdown();
    return 0;
}
