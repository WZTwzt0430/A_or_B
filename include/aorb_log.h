#ifndef AORB_LOG_H
#define AORB_LOG_H

/*
    A_or_B core library — 日志回传接口。

    本模块不写任何文件。它只负责把 DLL 内部发生的事，
    通过已注册的 sink 回调交给宿主（EXE）。

    为什么这样分层：
      - 写入者唯一（宿主），不会两个模块抢同一个文件
      - DLL 加载失败时，宿主自己的日志仍然能写
      - DLL 不依赖文件系统权限，也不关心日志目录在哪

    未注册 sink 时，emit 是空操作——DLL 照常工作，只是没人听。
*/

#include "aorb_export.h"

/* 日志级别，数值递增 */
#define AORB_LOG_DEBUG 0
#define AORB_LOG_INFO  1
#define AORB_LOG_WARN  2
#define AORB_LOG_ERROR 3

/*
    sink 回调签名。
      level  日志级别，见上面的 AORB_LOG_*
      tag    模块标记，定宽显示用，如 "core" / "save"
      msg    UTF-8 消息，已格式化
      user   注册时传入的透传指针

    回调在调用线程上同步执行。宿主实现里不要做耗时操作。
*/
typedef void (AORB_CALL *AorbLogSink)(int level, const char* tag, const char* msg, void* user);

/*
    注册 / 注销 sink。
      sink 传 NULL 表示注销。
      user 原样透传给每次回调。
    返回 0（成功）。
*/
AORB_EXTERN_C AORB_API int AORB_CALL aorb_log_set_sink(AorbLogSink sink, void* user);

/*
    发出一条日志。一般由 DLL 内部调用，宿主也可用来注入事件。
      tag  可为 NULL，此时记为 "core"
      msg  可为 NULL，此时记为 ""
*/
AORB_EXTERN_C AORB_API void AORB_CALL aorb_log_emit(int level, const char* tag, const char* msg);

/*
    低于 level 的消息被丢弃，默认 AORB_LOG_INFO。
    这是 DLL 侧的前置过滤；宿主的 sink 里还可以再过滤一次。
*/
AORB_EXTERN_C AORB_API void AORB_CALL aorb_log_set_level(int level);
AORB_EXTERN_C AORB_API int  AORB_CALL aorb_log_get_level(void);

#endif /* AORB_LOG_H */
