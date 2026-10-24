/*
    A_or_B core library — implementation.
    SPDX-License-Identifier: GPL-3.0

    关卡数据全部编译在本库里：剧情正文、选项文字、跳转目标都是 C++ 表，
    运行时不读任何外部文件。改剧情 = 改下面的关卡表 + 重新编译。

    存档仍然写 JSON（data/save.json），因为那是运行期数据，不是关卡内容。

    实现注意：
      1. kScenes 是静态数组，索引里存的是指向数组元素的指针，进程内永久有效。
      2. 所有返回 const char* 的接口，若内容需要拼接，先存进 g.bufXxx 再返回，
         否则返回临时串的悬空指针。
*/
#define AORB_CORE_EXPORTS
#include "aorb_core.h"
#include "aorb_log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "json.hpp"

namespace {

using json = nlohmann::json;

// ============================================================
//  关卡表
//
//  想加一关：在下面加一个正文数组 + 一个选项数组，再往 kScenes 里加一行。
//  想改剧情：直接改字符串，不用碰其它任何文件。
// ============================================================

struct Choice {
    const char* key;      // 单个大写字母
    const char* text;
    const char* target;   // 目标关卡 id
};

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

/* ---------------- 第 1 关 ---------------- */
static const char* const kElev1Content[] = {
    "你在电梯里醒来。",
    "金属墙壁，头顶一盏灯在闪。",
    "控制面板上只有两个按钮：A 和 B。",
    "没有楼层数字，没有开门键，没有警铃。",
    "",
    "你不记得自己是怎么进来的。",
};
static const Choice kElev1Choices[] = {
    { "A", "按 A", "park_1" },
    { "B", "按 B", "roof_1" },
};

/* ---------------- 第 2 关 ---------------- */
static const char* const kPark1Content[] = {
    "门开了。",
    "",
    "外面是地下停车场，日光灯管坏了一半，剩下的在嗡嗡响。",
    "水泥柱上刷着「B3」，字迹已经剥落。",
    "远处停着一辆车，车灯亮着，发动机没熄。",
    "排气管还在冒白烟。",
    "",
    "没有别人。",
};
static const Choice kPark1Choices[] = {
    { "A", "上车", "end_car" },
    { "B", "往深处走", "ease_1" },
};

/* ---------------- 第 3 关 ---------------- */
static const char* const kRoof1Content[] = {
    "门开了。",
    "",
    "风灌进来，带着雨的味道。",
    "城市的灯在脚下铺开，一直铺到看不见的地方。",
    "天台边缘站着一个人，背对着你。",
    "他不动，风把他的衣角吹得很响。",
    "",
    "你喊了一声。",
    "他没回头。",
};
static const Choice kRoof1Choices[] = {
    { "A", "走过去", "end_face" },
    { "B", "退回电梯", "elev_1" },
};

/* ---------------- 彩蛋关（显示为「第1关?」） ---------------- */
static const char* const kEase1Content[] = {
    "你往停车场深处走。",
    "",
    "灯越来越稀，走到最后完全黑了。",
    "你摸到一扇门，推开——",
    "",
    "是你醒来时的那部电梯。",
    "面板上，A 和 B 中间，多了一个没有标记的按钮。",
};
static const Choice kEase1Choices[] = {
    { "A", "按 A",      "park_1" },
    { "B", "按 B",      "roof_1" },
    { "C", "按 ???",    "end_elev" },
    { "D", "退回停车场", "park_1" },
};

/* ---------------- 结局：开到尽头 ---------------- */
static const char* const kEndCarContent[] = {
    "你上了车。",
    "",
    "座椅是热的，像刚有人坐过。",
    "后视镜里，电梯门正在合上。",
    "你这才发现，电梯里一直站着一个人——",
    "他一直都在，只是你回头的时候才看见。",
    "",
    "你没有回头。",
    "你踩下油门。",
    "",
    "前面是出口，亮着白光。",
};

/* ---------------- 结局：转身 ---------------- */
static const char* const kEndFaceContent[] = {
    "你走过去。",
    "",
    "脚步声被风吞掉了。",
    "走到一半，那个人开口了：",
    "「你终于来了。」",
    "",
    "他转过身。",
    "是你自己。",
    "——更准确地说，是很多年前的你。",
    "",
    "他说：「选 A 还是 B？」",
};

/* ---------------- 结局：电梯 ---------------- */
static const char* const kEndElevContent[] = {
    "你按下了那个没有标记的按钮。",
    "",
    "电梯开始震动。",
    "灯闪了几下，停了。",
    "稍后，电梯猛然下坠。",
    "",
    "你尖叫着，眼前一片黑。",
    "然后，什么都没有了。",
};

/* ---------------- 全部关卡 ---------------- */
static const Scene kScenes[] = {
    { "elev_1",  1,  1, 0, nullptr, "只有两个按钮", kElev1Content,    6, kElev1Choices, 2 },
    { "park_1",  2,  2, 0, nullptr, "地下三层",     kPark1Content,    8, kPark1Choices, 2 },
    { "roof_1",  2,  3, 0, nullptr, "顶楼",         kRoof1Content,    9, kRoof1Choices, 2 },
    { "ease_1",  3, -1, 1, "?",     "第三个按钮",   kEase1Content,    7, kEase1Choices, 4 },
    { "end_car", 4,  0, 0, nullptr, "开到尽头",     kEndCarContent,  11, nullptr,       0 },
    { "end_face",4,  0, 0, nullptr, "转身",         kEndFaceContent, 11, nullptr,       0 },
    { "end_elev",4,  0, 0, nullptr, "电梯",         kEndElevContent,  8, nullptr,       0 },
};

/* 起始关卡 */
static const char* const kEntryId = "elev_1";

// ============================================================
//  全局状态
// ============================================================

struct State {
    bool initialized = false;

    std::string savePath = "data/save.sav";
    std::string lastError;

    std::unordered_map<std::string, const Scene*> index;
    const Scene* current = nullptr;

    // 上次自动备份的时间点（time_t）。用于按间隔节流。
    std::time_t lastBackup = 0;
    // 自动备份间隔（秒），0 = 关闭。默认 10 分钟，宿主可用
    // aorb_backup_set_interval() 改。
    int backupInterval = 600;

    // 返回给外部的字符串缓存（每个 getter 一个，互不干扰）
    std::string bufId;
    std::string bufLabel;
    std::string bufTitle;
    std::string bufContent;
    std::string bufKey;
    std::string bufText;
};

State g;

const Scene* findScene(const std::string& id) {
    auto it = g.index.find(id);
    return it == g.index.end() ? nullptr : it->second;
}

std::string nowString() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void setError(const std::string& msg) { g.lastError = msg; }
void clearError() { g.lastError.clear(); }

// 回传给宿主的日志。宿主没注册 sink 时 aorb_log_emit 是空操作。
void logAt(int level, const char* tag, const std::string& msg) {
    aorb_log_emit(level, tag, msg.c_str());
}

// 出错时统一"记错误 + 写日志"
void failAt(int level, const char* tag, const std::string& msg) {
    setError(msg);
    logAt(level, tag, msg);
}

void copyTo(char* dst, size_t cap, const std::string& src) {
    if (cap == 0) return;
    std::snprintf(dst, cap, "%s", src.c_str());
}

// ---------- 存档路径 ----------
//   data/save.sav      主存档
//   data/save.bak      上一份存档（每次写入前自动备份）
//   data/save.json     旧格式，仅用于首次迁移

std::string backupPathOf(const std::string& p) {
    std::filesystem::path fs(p);
    fs.replace_extension(".bak");
    return fs.string();
}

std::string legacyPathOf(const std::string& p) {
    std::filesystem::path fs(p);
    fs.replace_extension(".json");
    return fs.string();
}

void resetSaveData(AorbSaveData* out) {
    std::memset(out, 0, sizeof(*out));
    out->valid = 0;
    out->clean = 1;
}

// 读一个存档文件；读到有效内容返回 true，out 已填好。
// 文件不存在 / JSON 坏了 / 没有 level 字段，都返回 false（不算错误）。
bool readSaveFile(const std::string& path, AorbSaveData* out) {
    std::ifstream in(path);
    if (!in) return false;

    try {
        json j;
        in >> j;
        if (j.contains("level") && j.at("level").is_string())
            copyTo(out->level, sizeof(out->level), j.at("level").get<std::string>());
        if (j.contains("title") && j.at("title").is_string())
            copyTo(out->title, sizeof(out->title), j.at("title").get<std::string>());
        if (j.contains("time") && j.at("time").is_string())
            copyTo(out->time, sizeof(out->time), j.at("time").get<std::string>());
        if (j.contains("clean") && j.at("clean").is_boolean())
            out->clean = j.at("clean").get<bool>() ? 1 : 0;

        out->valid = (out->level[0] != '\0') ? 1 : 0;
        return out->valid != 0;
    } catch (...) {
        return false;
    }
}

// 把一份存档数据写成文件；失败返回 false（调用方决定记什么日志）
bool writeSaveFile(const std::string& path, const AorbSaveData& d) {
    json j;
    j["level"] = d.level;
    j["title"] = d.title;
    j["time"]  = d.time;
    j["clean"] = (d.clean != 0);

    std::ofstream out(path);
    if (!out) return false;
    out << j.dump(2) << "\n";
    return static_cast<bool>(out);
}

// 文件最后写入时间；文件不存在返回 time_point::min()
std::filesystem::file_time_type fileTime(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    return ec ? std::filesystem::file_time_type::min() : t;
}

}  // namespace

// ============================================================
//  生命周期
// ============================================================

extern "C" AORB_API int AORB_CALL aorb_init(const char* savePath) {
    clearError();
    g.index.clear();
    g.current = nullptr;

    if (savePath != nullptr && savePath[0] != '\0') g.savePath = savePath;

    for (const Scene& s : kScenes) {
        g.index.emplace(s.id, &s);
    }

    if (g.index.find(kEntryId) == g.index.end()) {
        // 只会在关卡表被改坏时发生
        failAt(AORB_LOG_ERROR, "core", "built-in scene table has no entry");
        g.initialized = false;
        return AORB_ERR;
    }

    g.initialized = true;
    g.lastBackup = 0;                    // 0 = 下次进关卡时立即备份一次
    logAt(AORB_LOG_INFO, "core",
          "built-in scenes loaded: " + std::to_string(g.index.size()) +
          ", entry=" + kEntryId);
    return AORB_OK;
}

extern "C" AORB_API void AORB_CALL aorb_shutdown(void) {
    logAt(AORB_LOG_INFO, "core", "shutdown");
    g.index.clear();
    g.current = nullptr;
    g.initialized = false;
}

extern "C" AORB_API const char* AORB_CALL aorb_last_error(void) {
    return g.lastError.c_str();
}

extern "C" AORB_API int AORB_CALL aorb_api_version(void) {
    return AORB_API_VERSION;
}

// ============================================================
//  流程控制
// ============================================================

extern "C" AORB_API const char* AORB_CALL aorb_new_game(void) {
    clearError();
    if (!g.initialized) { failAt(AORB_LOG_ERROR, "core", "not initialized"); return nullptr; }

    g.current = findScene(kEntryId);
    if (g.current == nullptr) {
        failAt(AORB_LOG_ERROR, "core", "entry scene missing");
        return nullptr;
    }

    g.bufId = g.current->id;
    logAt(AORB_LOG_INFO, "core", "new game at " + g.bufId);
    return g.bufId.c_str();
}

extern "C" AORB_API const char* AORB_CALL aorb_continue_game(void) {
    clearError();
    if (!g.initialized) { failAt(AORB_LOG_ERROR, "core", "not initialized"); return nullptr; }

    AorbSaveData d{};
    if (aorb_save_read(&d) != AORB_OK || !d.valid) {
        logAt(AORB_LOG_DEBUG, "core", "no usable save, continue declined");
        return nullptr;
    }

    const Scene* s = findScene(d.level);
    if (s == nullptr) {
        // 存档保留，由调用方决定退回新游戏
        failAt(AORB_LOG_WARN, "core",
               "saved scene no longer exists (save kept): " + std::string(d.level));
        return nullptr;
    }
    g.current = s;
    g.bufId = g.current->id;
    logAt(AORB_LOG_INFO, "core", "resumed at " + g.bufId);
    return g.bufId.c_str();
}

extern "C" AORB_API const char* AORB_CALL aorb_current_scene(void) {
    if (g.current == nullptr) return "";
    g.bufId = g.current->id;
    return g.bufId.c_str();
}

extern "C" AORB_API int AORB_CALL aorb_is_ending(void) {
    if (g.current == nullptr) return 0;
    return g.current->choiceCount == 0 ? 1 : 0;
}

// ============================================================
//  展示信息
// ============================================================

extern "C" AORB_API const char* AORB_CALL aorb_scene_label(void) {
    if (g.current == nullptr) return "";
    const Scene& s = *g.current;

    // 结局关（type 4）不显示关号
    if (s.type == 4) {
        g.bufLabel = "结局";
        return g.bufLabel.c_str();
    }

    std::ostringstream oss;

    // fakeLevel 覆盖真实关号
    const int shown = (s.fakeLevel != 0) ? s.fakeLevel : s.level;
    if (shown > 0) oss << "第" << shown << "关";

    // fakeLevel2 追加后缀
    if (s.fakeLevel2 != nullptr) oss << s.fakeLevel2;

    g.bufLabel = oss.str();
    return g.bufLabel.c_str();
}

extern "C" AORB_API const char* AORB_CALL aorb_scene_title(void) {
    if (g.current == nullptr) return "";
    g.bufTitle = g.current->title;
    return g.bufTitle.c_str();
}

extern "C" AORB_API const char* AORB_CALL aorb_scene_content(void) {
    if (g.current == nullptr) return "";
    const Scene& s = *g.current;

    g.bufContent.clear();
    for (int i = 0; i < s.contentCount; ++i) {
        if (i > 0) g.bufContent += '\n';
        if (s.content[i] != nullptr) g.bufContent += s.content[i];
    }
    return g.bufContent.c_str();
}

// ============================================================
//  选项
// ============================================================

extern "C" AORB_API int AORB_CALL aorb_choice_count(void) {
    return (g.current != nullptr) ? g.current->choiceCount : 0;
}

extern "C" AORB_API const char* AORB_CALL aorb_choice_key(int index) {
    if (g.current == nullptr) return nullptr;
    if (index < 0 || index >= g.current->choiceCount) return nullptr;
    g.bufKey = g.current->choices[index].key;
    return g.bufKey.c_str();
}

extern "C" AORB_API const char* AORB_CALL aorb_choice_text(int index) {
    if (g.current == nullptr) return nullptr;
    if (index < 0 || index >= g.current->choiceCount) return nullptr;
    g.bufText = g.current->choices[index].text;
    return g.bufText.c_str();
}

extern "C" AORB_API const char* AORB_CALL aorb_choose(int index) {
    clearError();
    if (g.current == nullptr) { failAt(AORB_LOG_ERROR, "core", "no current scene"); return nullptr; }

    if (index < 0 || index >= g.current->choiceCount) {
        failAt(AORB_LOG_ERROR, "core",
               "choice index out of range: " + std::to_string(index));
        return nullptr;
    }

    const Choice& choice = g.current->choices[index];
    const Scene* next = findScene(choice.target);
    if (next == nullptr) {
        failAt(AORB_LOG_ERROR, "core",
               "target scene missing: " + std::string(choice.target));
        return nullptr;
    }

    const std::string from = g.current->id;
    g.current = next;
    g.bufId = g.current->id;
    logAt(AORB_LOG_INFO, "core",
          from + " -> " + g.bufId + " (choice " + std::to_string(index) + ")");
    return g.bufId.c_str();
}

// ============================================================
//  存档
//
//  两个文件，用途不同：
//    data/save.sav  正式存档。玩家主动「保存并退出」时写入，clean = 1。
//    data/save.bak  自动备份。游戏进行中按间隔（默认 10 分钟，可调）落盘，
//                   clean = 0；玩家主动保存后作废并删除。
//
//  读取时比较两个文件的修改时间，谁新用谁：崩溃前最后一次自动备份
//  可能比上次手动保存还新，此时用 .bak 恢复，而它的 clean = 0，
//  宿主据此提示「上次未正常退出」。
// ============================================================

extern "C" AORB_API int AORB_CALL aorb_save_read(AorbSaveData* out) {
    clearError();
    if (out == nullptr) { failAt(AORB_LOG_ERROR, "save", "out is null"); return AORB_ERR; }

    const std::string bak = backupPathOf(g.savePath);
    const std::string legacy = legacyPathOf(g.savePath);

    // 收集主存档与备份，按修改时间从新到旧
    struct Cand {
        const std::string* path;
        std::filesystem::file_time_type when;
    };
    std::vector<Cand> cands;
    const auto never = std::filesystem::file_time_type::min();

    const auto tSav = fileTime(g.savePath);
    if (tSav != never) cands.push_back({ &g.savePath, tSav });
    const auto tBak = fileTime(bak);
    if (tBak != never) cands.push_back({ &bak, tBak });

    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.when > b.when; });

    for (const Cand& c : cands) {
        resetSaveData(out);
        if (!readSaveFile(*c.path, out)) {
            logAt(AORB_LOG_WARN, "save", "unreadable save file skipped: " + *c.path);
            continue;
        }
        if (*c.path == bak) {
            logAt(AORB_LOG_WARN, "save",
                  "using newer auto backup: " + bak +
                  " (level=" + std::string(out->level) + ")");
        } else {
            logAt(AORB_LOG_INFO, "save",
                  "save loaded: " + std::string(out->level) +
                  (out->clean ? " (clean exit)" : " (incomplete)"));
        }
        return AORB_OK;
    }

    // 旧格式迁移：没有 .sav / .bak，但存在旧的 save.json
    resetSaveData(out);
    if (readSaveFile(legacy, out)) {
        logAt(AORB_LOG_INFO, "save", "migrating legacy save: " + legacy);
        if (!writeSaveFile(g.savePath, *out))
            logAt(AORB_LOG_WARN, "save", "migration write failed: " + g.savePath);
        return AORB_OK;
    }

    resetSaveData(out);
    logAt(AORB_LOG_DEBUG, "save", "no save file");
    return AORB_OK;
}

extern "C" AORB_API int AORB_CALL aorb_save_write(int clean) {
    clearError();
    if (g.current == nullptr) { failAt(AORB_LOG_ERROR, "save", "no current scene"); return AORB_ERR; }

    // ---- 自动备份（clean = 0）：按间隔节流，不碰正式存档 ----
    if (clean == 0) {
        if (g.backupInterval <= 0) {
            logAt(AORB_LOG_DEBUG, "save", "auto backup disabled");
            return AORB_OK;
        }
        const std::time_t now = std::time(nullptr);
        if (g.lastBackup != 0 && now - g.lastBackup < g.backupInterval) {
            logAt(AORB_LOG_DEBUG, "save",
                  "auto backup skipped, " + std::to_string(now - g.lastBackup) +
                  "s < " + std::to_string(g.backupInterval) + "s");
            return AORB_OK;
        }
        g.lastBackup = now;

        AorbSaveData d{};
        copyTo(d.level, sizeof(d.level), g.current->id);
        copyTo(d.title, sizeof(d.title), g.current->title);
        copyTo(d.time,  sizeof(d.time),  nowString());
        d.valid = 1;
        d.clean = 0;

        const std::string bak = backupPathOf(g.savePath);
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(bak).parent_path(), ec);

        if (!writeSaveFile(bak, d)) {
            // 备份失败不影响游戏继续
            logAt(AORB_LOG_WARN, "save", "auto backup failed: " + bak);
            return AORB_OK;
        }
        logAt(AORB_LOG_INFO, "save", "auto backup: " + std::string(g.current->id));
        return AORB_OK;
    }

    // ---- 正式存档（clean = 1）：写 .sav，并让自动备份作废 ----
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(g.savePath).parent_path(), ec);

    AorbSaveData d{};
    copyTo(d.level, sizeof(d.level), g.current->id);
    copyTo(d.title, sizeof(d.title), g.current->title);
    copyTo(d.time,  sizeof(d.time),  nowString());
    d.valid = 1;
    d.clean = 1;

    if (!writeSaveFile(g.savePath, d)) {
        failAt(AORB_LOG_ERROR, "save", "cannot write save file: " + g.savePath);
        return AORB_ERR;
    }

    const std::string bak = backupPathOf(g.savePath);
    const bool hadBackup = std::filesystem::exists(bak, ec);
    if (hadBackup) std::filesystem::remove(bak, ec);

    g.lastBackup = std::time(nullptr);   // 保存后重新计时
    logAt(AORB_LOG_INFO, "save",
          "save written: " + std::string(g.current->id) +
          (hadBackup ? " (clean, backup removed)" : " (clean)"));
    return AORB_OK;
}

extern "C" AORB_API int AORB_CALL aorb_save_clear(void) {
    clearError();

    // 三个文件一起清：只删 .sav 会让旧的 .bak 或 save.json 在下次读取时复活
    int removed = 0;
    for (const std::string& p : { g.savePath, backupPathOf(g.savePath), legacyPathOf(g.savePath) }) {
        std::error_code ec;
        if (std::filesystem::remove(p, ec)) ++removed;
        if (ec) {
            failAt(AORB_LOG_ERROR, "save", "cannot remove " + p + ": " + ec.message());
            return AORB_ERR;
        }
    }
    g.lastBackup = 0;                    // 下一次进关卡重新备份
    logAt(AORB_LOG_INFO, "save", "save cleared (" + std::to_string(removed) + " file(s))");
    return AORB_OK;
}

// ============================================================
//  自动备份间隔
// ============================================================

extern "C" AORB_API void AORB_CALL aorb_backup_set_interval(int seconds) {
    g.backupInterval = (seconds > 0) ? seconds : 0;
    logAt(AORB_LOG_INFO, "save",
          (g.backupInterval > 0)
              ? "backup interval = " + std::to_string(g.backupInterval) + "s"
              : "backup interval disabled");
}

extern "C" AORB_API int AORB_CALL aorb_backup_get_interval(void) {
    return g.backupInterval;
}
