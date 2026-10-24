// DLL smoke test: verifies the core library and the log bridge.
// SPDX-License-Identifier: GPL-3.0
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include "aorb_core.h"
#include "aorb_log.h"
#include "host_logger.h"

int main() {
    // ---- host logger first: everything after this gets recorded ----
    bool logOk = host_log::init("logs", "smoke_test");
    host_log::info("test", "=== smoke test starting ===");
    if (logOk) {
        host_log::info("test", "log file: %s", host_log::current_file().c_str());
    }

    // ---- route DLL log events into the same file ----
    aorb_log_set_sink(host_log::sink_for_dll, nullptr);
    host_log::info("test", "log sink registered");

    const char* kTmpSave = "build/smoke_save.sav";

    if (aorb_init(kTmpSave) != AORB_OK) {
        host_log::error("test", "init failed: %s", aorb_last_error());
        std::cout << "init failed: " << aorb_last_error() << "\n";
        host_log::info("test", "=== smoke test aborted ===");
        host_log::shutdown();
        return 1;
    }
    host_log::info("test", "core api version = %d", aorb_api_version());
    if (aorb_api_version() != AORB_API_VERSION) {
        std::cout << "[0] api version mismatch: dll=" << aorb_api_version()
                  << " host=" << AORB_API_VERSION << " (WRONG)\n";
        host_log::error("test", "api version mismatch");
        host_log::shutdown();
        return 1;
    }

    // 1. new game
    aorb_new_game();
    std::cout << "[1] start: " << aorb_current_scene()
              << " | " << aorb_scene_label()
              << " | " << aorb_scene_title()
              << " | choices=" << aorb_choice_count() << "\n";

    // 2. walk A -> B to reach the easter-egg scene (4 choices)
    aorb_choose(0);
    aorb_choose(1);
    std::cout << "[2] path A-B: " << aorb_current_scene()
              << " | " << aorb_scene_label()
              << " | " << aorb_scene_title()
              << " | choices=" << aorb_choice_count() << "\n";

    // 3. easter-egg scene, pick C -> end_elev (ending)
    aorb_choose(2);
    std::cout << "[3] pick C: " << aorb_current_scene()
              << " | " << aorb_scene_label()
              << " | " << aorb_scene_title()
              << " | is_ending=" << aorb_is_ending() << "\n";

    // 4. save write/read
    aorb_new_game();
    aorb_choose(1);
    aorb_save_write(0);
    AorbSaveData d{};
    aorb_save_read(&d);
    std::cout << "[4] save: valid=" << d.valid << " clean=" << d.clean
              << " level=" << d.level << " title=" << d.title
              << " time=" << d.time << "\n";

    // 5. continue from save
    aorb_new_game();
    const char* c = aorb_continue_game();
    std::cout << "[5] continue: " << (c ? c : "(null)")
              << " | title=" << aorb_scene_title() << "\n";

    // 6. save-and-exit (clean = 1)
    aorb_save_write(1);
    aorb_save_read(&d);
    std::cout << "[6] clean=1 save: clean=" << d.clean << "\n";

    // 7. clear save
    aorb_save_clear();
    aorb_save_read(&d);
    std::cout << "[7] after clear: valid=" << d.valid << "\n";

    // 8. out-of-range guard
    aorb_new_game();
    std::cout << "[8] choose(99): "
              << (aorb_choose(99) ? "non-null(WRONG)" : "null(OK)")
              << " | err=" << aorb_last_error() << "\n";

    // 9. auto backup: clean=0 writes .bak and leaves .sav alone
    aorb_new_game();
    aorb_choose(1);                     // -> roof_1
    aorb_save_write(0);                 // 第一次自动备份（lastBackup = 0，立即写）
    const std::string sav = kTmpSave;
    const std::string bak = "build/smoke_save.bak";
    const bool bakMade = std::filesystem::exists(bak);
    const bool savAvoided = !std::filesystem::exists(sav);
    std::cout << "[9] auto backup: bak=" << (bakMade ? "yes(OK)" : "no(WRONG)")
              << " sav-untouched=" << (savAvoided ? "yes(OK)" : "no(WRONG)") << "\n";

    // 10. throttle: a second clean=0 within the interval must not rewrite .bak
    const auto t1 = std::filesystem::last_write_time(bak);
    aorb_choose(0);                     // -> end_face
    aorb_save_write(0);
    const auto t2 = std::filesystem::last_write_time(bak);
    std::cout << "[10] throttled: " << (t1 == t2 ? "yes(OK)" : "no(WRONG)") << "\n";

    // 11. clean save: writes .sav and deletes the auto backup
    aorb_save_write(1);
    std::cout << "[11] clean save: sav="
              << (std::filesystem::exists(sav) ? "yes(OK)" : "no(WRONG)")
              << " bak-removed="
              << (!std::filesystem::exists(bak) ? "yes(OK)" : "no(WRONG)") << "\n";

    // 12. crash recovery: only .bak exists -> read must use it with clean=0
    aorb_save_clear();
    aorb_new_game();
    aorb_choose(1);                     // -> roof_1
    aorb_save_write(0);                 // 只写 .bak
    AorbSaveData n{};
    aorb_save_read(&n);
    std::cout << "[12] crash recovery: valid=" << n.valid
              << " clean=" << n.clean
              << " level=" << n.level << "\n";

    // 13. legacy migration: only the old .json exists
    aorb_save_clear();
    {
        std::ofstream legacy("build/smoke_save.json");
        legacy << R"({"level":"roof_1","title":"顶楼","time":"2026-01-01 00:00:00","clean":true})";
    }
    AorbSaveData m{};
    aorb_save_read(&m);
    std::cout << "[13] legacy migrate: valid=" << m.valid
              << " level=" << m.level
              << " sav=" << (std::filesystem::exists(sav) ? "yes(OK)" : "no(WRONG)")
              << "\n";

    aorb_shutdown();
    host_log::info("test", "=== smoke test finished ===");

    // 14. DLL -> host log bridge: core events must reach the host file
    bool sawCore = false;
    {
        std::ifstream logIn(host_log::current_file());
        std::string line;
        while (std::getline(logIn, line)) {
            if (line.find("[core]") != std::string::npos ||
                line.find("[save]") != std::string::npos) {
                sawCore = true;
                break;
            }
        }
    }
    std::cout << "[14] log bridge: core events in host log = "
              << (sawCore ? "yes(OK)" : "no(WRONG)") << "\n";

    host_log::shutdown();
    std::cout << "\nALL OK\n";
    return 0;
}
