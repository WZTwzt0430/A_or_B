/*
    A_or_B core library — 日志回传实现。
    SPDX-License-Identifier: GPL-3.0

    本文件不写文件、不开目录、不碰磁盘。
    它只做三件事：存 sink、按级别过滤、调用 sink。

    注意：调用 sink 时不持锁。否则宿主回调里若再 emit
    就会自死锁。先把 sink / user 拷出来，出作用域再调。
*/
#define AORB_CORE_EXPORTS
#include "aorb_log.h"

#include <mutex>

namespace {

/* 成功返回 0（与 aorb_core.h 的 AORB_OK 同值）。
   本模块不包含 core 头文件：日志桥接不应依赖关卡库。 */
constexpr int kOk = 0;

std::mutex g_mutex;
AorbLogSink g_sink     = nullptr;
void*       g_user     = nullptr;
int         g_minLevel = AORB_LOG_INFO;

}  // namespace

extern "C" AORB_API int AORB_CALL aorb_log_set_sink(AorbLogSink sink, void* user) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sink = sink;
    g_user = user;
    return kOk;
}

extern "C" AORB_API void AORB_CALL aorb_log_emit(int level, const char* tag, const char* msg) {
    AorbLogSink sink;
    void* user;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_sink == nullptr) return;      // 没人听，直接丢
        if (level < g_minLevel) return;     // 前置过滤
        sink = g_sink;
        user = g_user;
    }
    // 不持锁调用，允许宿主回调里再 emit
    sink(level, (tag != nullptr) ? tag : "core",
                (msg != nullptr) ? msg : "", user);
}

extern "C" AORB_API void AORB_CALL aorb_log_set_level(int level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_minLevel = level;
}

extern "C" AORB_API int AORB_CALL aorb_log_get_level(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_minLevel;
}
