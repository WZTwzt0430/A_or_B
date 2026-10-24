#ifndef AORB_EXPORT_H
#define AORB_EXPORT_H

/*
    A_or_B — 跨模块导出宏。aorb_core.h / aorb_log.h 共用。

    编译 DLL 时定义 AORB_CORE_EXPORTS，符号为 dllexport；
    编译 EXE 时不定义，同一批头文件里的符号自动变成 dllimport。

    AORB_CALL 固定 __cdecl：DLL 与 EXE 必须使用同一调用约定，
    否则栈会错位。这是跨模块最容易踩的坑之一。
*/

#ifdef __cplusplus
#  define AORB_EXTERN_C extern "C"
#else
#  define AORB_EXTERN_C
#endif

#if defined(_WIN32)
#  ifdef AORB_CORE_EXPORTS
#    define AORB_API __declspec(dllexport)
#  else
#    define AORB_API __declspec(dllimport)
#  endif
#  define AORB_CALL __cdecl
#else
#  define AORB_API
#  define AORB_CALL
#endif

/*
    API 版本。宿主启动时用 aorb_api_version() 探测。
    将来切换到显式加载（LoadLibrary + GetProcAddress）时，
    这个版本号是判断 DLL 是否兼容的唯一依据。

    历史：
      1  初版（剧情硬编码）
      2  关卡改为外部 JSON 加载，新增日志 sink
      3  关卡数据回到代码内置（编译进 DLL），aorb_init 去掉 levelPath
*/
#define AORB_API_VERSION 3

#endif /* AORB_EXPORT_H */
