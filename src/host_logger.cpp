/*
    宿主日志器实现。
    SPDX-License-Identifier: GPL-3.0

    写文件是这里唯一的重活。所有失败都静默降级：
    日志可以停，程序不能停。

    每次 init 都新建一个文件，文件名带启动时刻到秒，
    所以日志只增不改，历史记录永远在。
*/
#include "host_logger.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>

namespace {

std::mutex    g_mutex;
std::mutex    g_pathMutex;         // 只保护 g_path，可被回调线程安全读取
std::ofstream g_out;
std::string   g_dir     = "logs";
std::string   g_path;
int           g_minLevel = host_log::LevelInfo;
bool          g_ready    = false;  // 目录可用且文件已打开

const char* levelName(int level) {
    switch (level) {
        case host_log::LevelDebug: return "DEBUG";
        case host_log::LevelInfo:  return "INFO ";
        case host_log::LevelWarn:  return "WARN ";
        case host_log::LevelError: return "ERROR";
        default:                   return "?????";
    }
}

std::tm localNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    return tm;
}

// 每行行首的时间戳：YYYY-MM-DD HH:MM:SS
std::string nowStamp() {
    const std::tm tm = localNow();
    char s[40]{};
    std::snprintf(s, sizeof(s), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return s;
}

// 文件名用的时间戳：YYYYMMDD-HHMMSS
std::string fileStamp() {
    const std::tm tm = localNow();
    char s[32]{};
    std::snprintf(s, sizeof(s), "%04d%02d%02d-%02d%02d%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return s;
}

// 打开一个新文件。调用方须已持锁。
// 文件名带秒级时间戳；若同名文件已存在（同一秒重复启动），
// 依次尝试 -2、-3……号，绝不覆盖已有日志。
bool ensureOpenLocked() {
    if (g_ready && g_out.is_open()) return true;

    if (g_out.is_open()) g_out.close();

    std::error_code ec;
    std::filesystem::create_directories(g_dir, ec);
    if (ec) { g_ready = false; return false; }

    const std::string base = "aorb-" + fileStamp();
    std::filesystem::path p = std::filesystem::path(g_dir) / (base + ".log");

    for (int i = 2; i <= 99 && std::filesystem::exists(p, ec); ++i) {
        p = std::filesystem::path(g_dir) /
            (base + "-" + std::to_string(i) + ".log");
    }

    {
        std::lock_guard<std::mutex> lock(g_pathMutex);
        g_path = p.string();
    }

    // trunc：新文件从头写，不追加旧内容
    g_out.open(g_path, std::ios::out | std::ios::trunc);
    if (!g_out) { g_ready = false; return false; }

    g_ready = true;
    return true;
}

void writeLineLocked(int level, const char* tag, const char* msg) {
    if (!ensureOpenLocked()) return;

    g_out << nowStamp() << " [" << levelName(level) << "] ["
          << (tag ? tag : "app") << "] "
          << (msg ? msg : "") << "\n";
    g_out.flush();                       // 每行落盘，强退不丢
    if (!g_out) g_ready = false;         // 磁盘满 / 句柄失效，静默停摆
}

void formatAndWrite(int level, const char* tag, const char* fmt, std::va_list args) {
    char buf[1024]{};
    if (fmt != nullptr) {
        std::vsnprintf(buf, sizeof(buf), fmt, args);
    }
    host_log::write(level, tag, buf);
}

}  // namespace

namespace host_log {

bool init(const std::string& dir, const std::string& appTag) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!dir.empty()) g_dir = dir;

    if (g_out.is_open()) g_out.close();
    g_ready = false;
    {
        std::lock_guard<std::mutex> lock(g_pathMutex);
        g_path.clear();
    }

    if (!ensureOpenLocked()) return false;

    // 文件头横幅：这是哪个版本、什么时候启动的日志
    g_out << "===== A_or_B"
          << (appTag.empty() ? "" : (" " + appTag))
          << " | started " << nowStamp()
          << " | log " << (g_path) << " =====\n";
    g_out.flush();
    return static_cast<bool>(g_out);
}

void shutdown(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_out.is_open()) {
        g_out.flush();
        g_out.close();
    }
    g_ready = false;
}

void set_level(int level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_minLevel = level;
}

int get_level(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_minLevel;
}

std::string current_file(void) {
    // 用独立的小锁：调用方可能正在 sink 回调里（那时 g_mutex 已被 write() 持有），
    // 再抢 g_mutex 会自死锁。锁序固定为 g_mutex -> g_pathMutex。
    std::lock_guard<std::mutex> lock(g_pathMutex);
    return g_path;
}

void write(int level, const char* tag, const char* msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (level < g_minLevel) return;
    writeLineLocked(level, tag, msg);
}

void debug(const char* tag, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    formatAndWrite(LevelDebug, tag, fmt, args);
    va_end(args);
}

void info(const char* tag, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    formatAndWrite(LevelInfo, tag, fmt, args);
    va_end(args);
}

void warn(const char* tag, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    formatAndWrite(LevelWarn, tag, fmt, args);
    va_end(args);
}

void error(const char* tag, const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    formatAndWrite(LevelError, tag, fmt, args);
    va_end(args);
}

void sink_for_dll(int level, const char* tag, const char* msg, void* /*user*/) {
    write(level, tag, msg);
}

}  // namespace host_log
