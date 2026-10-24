/*
    A_or_B — EXE 侧参考实现（调用 aorb_core.dll）。
    SPDX-License-Identifier: GPL-3.0

    职责边界：
      本文件只管 显示、按键、清屏、颜色。
      剧情、跳转、存档全在 DLL 里，这里一行都不碰。

    接入方式：把本文件的内容搬进 src\main.cpp，
    或直接用它替换 src\main.cpp 的主体。
*/
#include <iostream>
#include <string>
#include <cctype>
#include <cstdlib>
#include <conio.h>
#include <Windows.h>
#include "aorb_core.h"
#include "aorb_log.h"
#include "host_logger.h"
#include "console_color.h"

using namespace console_color;

namespace {

constexpr int kEsc = 27;

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
    int n = aorb_choice_count();
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
        int raw = _getch();
        if (raw == kEsc) return -1;

        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        for (int i = 0; i < n; ++i) {
            const char* k = aorb_choice_key(i);
            if (k && k[0] == c) return i;
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
        int c = _getch();
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
        int c = _getch();
        if (c == '1') {
            std::cout << "1\n";
            aorb_save_write(1);                    // clean = 1
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

// 命令行参数：
//   --backup-interval=<秒>   自动备份间隔，0 关闭。方便测试时缩短。
static int backupIntervalFromArgs(int argc, char** argv) {
    const std::string k = "--backup-interval=";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a.rfind(k, 0) == 0) {
            return std::atoi(a.c_str() + k.size());
        }
    }
    return -1;   // 未指定，用 DLL 默认值
}

int main(int argc, char** argv) {
    // 日志先于一切初始化：即使后面 DLL 初始化失败，也要留下记录。
    // 横幅里写明版本与 core API，方便事后辨认这份日志属于哪个构建。
    const int api = aorb_api_version();
    host_log::init("logs", std::string("v1.1.0_Alpha | core api ") + std::to_string(api));
    host_log::set_level(host_log::LevelInfo);

    if (GetConsoleWindow() != nullptr && !SetConsoleOutputCP(CP_UTF8)) {
        host_log::error("host", "SetConsoleOutputCP(CP_UTF8) failed");
        MessageBoxW(nullptr, L"设置控制台输出编码为UTF-8失败。",
                    L"A_or_B程序错误", MB_OK | MB_ICONERROR);
        host_log::shutdown();
        return 1;
    }

    host_log::info("host", "A_or_B host starting");

    // DLL 内部的事件从这里汇入同一个日志文件
    aorb_log_set_sink(host_log::sink_for_dll, nullptr);

    // 先比对 API 版本：不匹配说明握手协议、结构体布局都可能对不上，
    // 这时候继续调用其余接口等于盲飞。
    host_log::info("host", "core api version = %d (host expects %d)",
                   api, AORB_API_VERSION);
    if (api != AORB_API_VERSION) {
        std::cout << "核心库版本不匹配：DLL 为 " << api
                  << "，本程序需要 " << AORB_API_VERSION << "\n";
        host_log::error("host", "core api version mismatch: dll=%d host=%d",
                        api, AORB_API_VERSION);
        host_log::shutdown();
        return 1;
    }

    if (aorb_init("data/save.sav") != AORB_OK) {
        std::cout << "核心库初始化失败：" << aorb_last_error() << "\n";
        host_log::error("host", "core init failed: %s", aorb_last_error());
        host_log::shutdown();
        return 1;
    }

    // 允许命令行覆盖自动备份间隔，方便测试
    const int intervalArg = backupIntervalFromArgs(argc, argv);
    if (intervalArg >= 0) {
        aorb_backup_set_interval(intervalArg);
        host_log::info("host", "backup interval from cli = %ds", intervalArg);
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

        int n = showChoices();

        if (n == 0) {                              // 结局
            host_log::info("host", "reached ending at %s", aorb_current_scene());
            aorb_save_clear();
            std::cout << "\n（结束，按任意键退出）\n";
            _getch();
            break;
        }

        aorb_save_write(0);                        // 进场景就自动存档

        int pick = waitChoice(n);
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
    }

    host_log::info("host", "host exiting");
    aorb_shutdown();
    host_log::shutdown();
    return 0;
}
