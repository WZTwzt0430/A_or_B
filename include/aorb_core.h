/*
    A_or_B core library — public C ABI.
    SPDX-License-Identifier: GPL-3.0

    职责：关卡加载、剧情推进、存档读写。
    关卡数据来自外部 JSON（默认 data/levels/level.json），不在代码里写死。
    不涉及界面、颜色、按键——那些留在 EXE 一侧。

    字符串约定：
      所有返回 const char* 的函数，指针指向库内部缓存，
      在下一次调用「同一函数」或 aorb_shutdown() 之前有效。
      调用方不要保存指针，不要 free。文本一律 UTF-8。

    线程：非线程安全。控制台单线程使用即可。
*/
#ifndef AORB_CORE_H
#define AORB_CORE_H

/* AORB_API / AORB_CALL / AORB_EXTERN_C 与 AORB_API_VERSION 统一定义在这里，
   本头文件不再自行定义一份，避免两处宏定义漂移。 */
#include "aorb_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 返回码 */
#define AORB_OK          0
#define AORB_ERR        -1    /* 一般错误，见 aorb_last_error() */
#define AORB_NOT_FOUND  -2    /* 场景 id 不存在 */
#define AORB_NOT_READY  -3    /* 未 aorb_init() 或初始化失败 */

typedef struct AorbSaveData {
    int  valid;              /* 1 = 有有效存档；0 = 无（其余字段无意义） */
    int  clean;              /* 1 = 主动保存并退出；0 = 游戏进行中 */
    char level[128];         /* 场景 id */
    char title[256];         /* 场景标题，供恢复面板显示 */
    char time[32];           /* 形如 2026-10-06 18:01:23 */
} AorbSaveData;

/* ============================================================
   生命周期
   ============================================================ */

/* 初始化：建立关卡 id 索引。
   关卡数据全部编译在本库里，运行时不读任何外部文件。
   savePath  存档路径；传 NULL 用默认值 "data/save.sav"
             （同目录下自动维护 .bak 备份，并兼容旧的 .json 存档）
   返回 AORB_OK 或 AORB_ERR（详情见 aorb_last_error()） */
AORB_API int  AORB_CALL aorb_init(const char* savePath);

/* 释放内部数据。可重复调用。 */
AORB_API void AORB_CALL aorb_shutdown(void);

/* 最近一次错误描述；无错误返回空串。 */
AORB_API const char* AORB_CALL aorb_last_error(void);

/* 库的 API 版本，等于编译期 AORB_API_VERSION。
   宿主启动时先比对这个值，再决定是否继续调用其余接口。 */
AORB_API int AORB_CALL aorb_api_version(void);

/* ============================================================
   流程控制
   ============================================================ */

/* 从头开始。返回起点场景 id；失败返回 NULL。 */
AORB_API const char* AORB_CALL aorb_new_game(void);

/* 从存档继续。无有效存档返回 NULL（此时应改调 aorb_new_game）。
   存档指向的场景不存在时返回 NULL，存档保留不动。 */
AORB_API const char* AORB_CALL aorb_continue_game(void);

/* 当前场景 id；未开始时返回空串。 */
AORB_API const char* AORB_CALL aorb_current_scene(void);

/* 是否已走到结局（无后续选项）。1 = 是。 */
AORB_API int AORB_CALL aorb_is_ending(void);

/* ============================================================
   当前场景的展示信息
   ============================================================ */

/* 关号标签："第1关" / "第1关?" / "结局" */
AORB_API const char* AORB_CALL aorb_scene_label(void);

/* 场景标题 */
AORB_API const char* AORB_CALL aorb_scene_title(void);

/* 场景正文，多行用 '\n' 连接。调用方可直接整块输出。 */
AORB_API const char* AORB_CALL aorb_scene_content(void);

/* ============================================================
   选项
   ============================================================ */

/* 当前场景的选项数；0 表示结局 */
AORB_API int AORB_CALL aorb_choice_count(void);

/* 第 index 个选项的按键（单个大写字母），越界返回 NULL */
AORB_API const char* AORB_CALL aorb_choice_key(int index);

/* 第 index 个选项的文字，越界返回 NULL */
AORB_API const char* AORB_CALL aorb_choice_text(int index);

/* 选择第 index 个选项，推进到下一场景。
   成功返回新场景 id；越界或目标缺失返回 NULL。
   走到结局时新场景仍有效，用 aorb_is_ending() 判断。 */
AORB_API const char* AORB_CALL aorb_choose(int index);

/* ============================================================
   存档
   ============================================================ */

/* 读存档。out->valid 为 0 表示没有可用存档（不算错误）。
   会同时看 .sav（正式存档）和 .bak（自动备份），取较新的那个：
   崩溃前的自动备份可能比上次手动保存还新，此时返回的 clean = 0。 */
AORB_API int AORB_CALL aorb_save_read(AorbSaveData* out);

/* 写当前进度。
   clean != 0：正式存档，写 .sav 并删除自动备份。玩家主动保存时用。
   clean == 0：自动备份，写 .bak，按 aorb_backup_set_interval() 的间隔节流。 */
AORB_API int AORB_CALL aorb_save_write(int clean);

/* 删除存档。文件不存在也算成功。 */
AORB_API int AORB_CALL aorb_save_clear(void);

/* 自动备份间隔（秒）。默认 600（10 分钟），传 0 关闭自动备份。
   这只是默认值，宿主可以按需要改。 */
AORB_API void AORB_CALL aorb_backup_set_interval(int seconds);
AORB_API int  AORB_CALL aorb_backup_get_interval(void);

#ifdef __cplusplus
}
#endif

#endif /* AORB_CORE_H */
