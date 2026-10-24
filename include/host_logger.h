#ifndef HOST_LOGGER_H
#define HOST_LOGGER_H

/*
    宿主日志器 —— 编进 A_or_B.exe，是日志的唯一写入者。

    与 DLL 侧 aorb_log 的分工：
      aorb_log   ：DLL 内部产生事件，通过 sink 回传，自己不落盘
      host_logger：接收回传 + 记录 EXE 自己的事件，统一写入文件

    可靠性要点：
      - EXE 一进 main 就能写，不依赖 DLL 是否加载成功
      - 每行写完立即 flush，崩溃/强退不丢已写内容
      - 目录建不出来时静默降级，绝不让日志拖垮游戏
      - 内部加锁，将来若引入线程也安全

    文件路径：<dir>\aorb-YYYYMMDD-HHMMSS.log，每次启动新建一个，
    绝不覆盖历史日志。同一秒内重复启动会自动加 -2 / -3 后缀。

    文件开头会写一行横幅，标明这是哪个版本、什么时候启动的。
*/

#include <string>

namespace host_log {

/* 日志级别，数值与 DLL 侧 AORB_LOG_* 一一对应 */
enum Level {
    LevelDebug = 0,
    LevelInfo  = 1,
    LevelWarn  = 2,
    LevelError = 3
};

/*
    初始化。dir 为空则用 "logs"。
    appTag 写进文件头横幅，通常传版本号，如 "v1.1.0_Alpha"。
    返回 false 表示目录不可用——此时日志停摆，但程序应继续运行。
    可重复调用，重复调用会先关闭旧文件、再开一个新文件。
*/
bool init(const std::string& dir = "logs", const std::string& appTag = "");

/* 关闭文件。程序退出前调用；不调也不会丢数据（每行已 flush）。 */
void shutdown(void);

/* 低于 level 的消息丢弃，默认 LevelInfo */
void set_level(int level);
int  get_level(void);

/* 当前正在写的日志文件路径；未初始化时为空串。
   返回值是副本：调用方可以安全持有，内部加锁，不与你抢 g_path。 */
std::string current_file(void);

/*
    底层写入。一般用下面的 info/warn/error 便捷函数。
      tag  模块标记，可为 NULL
      msg  UTF-8 消息，可为 NULL
*/
void write(int level, const char* tag, const char* msg);

/* printf 风格便捷函数，自动格式化后调用 write */
void debug(const char* tag, const char* fmt, ...);
void info (const char* tag, const char* fmt, ...);
void warn (const char* tag, const char* fmt, ...);
void error(const char* tag, const char* fmt, ...);

/*
    注册给 DLL 的回调适配器。
    用法：aorb_log_set_sink(host_log::sink_for_dll, nullptr);
*/
void sink_for_dll(int level, const char* tag, const char* msg, void* user);

}  // namespace host_log

#endif /* HOST_LOGGER_H */
