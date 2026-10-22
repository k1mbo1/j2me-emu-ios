/* vm_internal.h - VM 内部共享（classfile/heap/interp/vm 之间），外部不要 include */
#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "vm.h"

#ifdef _WIN32
#  include <pthread.h>
#else
#  include <pthread.h>
#endif

/* ============ VM 全局状态 ============ */
typedef struct VMState {
    Class*      classes;            /* 全局类链 */
    VMThread*   threads;            /* 线程链 */
    pthread_mutex_t threadsMu;

    /* 类路径（jar 链表，游戏 jar 在前） */
    struct JarEntry* jars;

    /* native 注册表 */
    struct NativeEntry* natives;

    /* 字符串 intern 表 */
    struct InternEntry* intern;

    /* 全局根 */
    Object**    globals; int nGlobals, capGlobals;

    /* GC */
    Object*     heap;               /* 对象链 */
    uint64_t    allocatedBytes;
    uint64_t    gcThreshold;        /* 触发阈值 */
    uint32_t    gcMarkColor;
    int         gcDisabled;
    pthread_mutex_t gcMu;

    /* 内置异常单例（预分配，避免指令中途 GC） */
    Object*     excNPE, *excCCE, *excAIOOBE, *excArith, *excSIOOBE, *excOOM;

    /* 基础类缓存 */
    Class*      clsObject, *clsString, *clsClass, *clsThread;
    Class*      clsThrowable;
    Field*      fldStringValue;     /* String.value char[] */
    Field*      fldClassJavaName;   /* java/lang/Class.name String */

    /* 主 MIDlet */
    Object*     midlet;
    int         midletDestroyed;

    PlatformHooks hooks;
} VMState;

extern VMState vmG;

/* ============ classfile.c ============ */
Class* cfLoadClass(const char* name);            /* 读字节码并解析 */
int    cfLinkClass(Class* c);                    /* 计算字段布局 + vtable + 接口表 */
int    cfInitClass(VMThread* t, Class* c);       /* 执行 <clinit> */
void   cfInitVtables(Class* c);

/* 常量池解析：返回解析缓存，强制重解析传 force */
Class*  cpClass(VMThread* t, CpEntry* e, Class* self);
Field*  cpField(VMThread* t, CpEntry* e, Class* self, int isStatic);
Method* cpMethod(VMThread* t, CpEntry* e, Class* self);
Object* cpString(VMThread* t, CpEntry* e, Class* self);   /* intern 过的 String */
Method* vmFindMethodImpl(Class* start, const char* name, const char* desc);

/* ============ heap.c ============ */
Object*      heapAlloc(VMThread* t, Class* c, size_t extra);        /* 可能触发 GC */
Object*      heapAllocNoGC(VMThread* t, Class* c, size_t extra);    /* 绝不 GC（失败 abort） */
void         heapMaybeGC(VMThread* t);
void         heapGC(VMThread* t);
int          heapAnyThreadGcDisabled(void);
Monitor*     objMonitor(Object* o);
void         monEnter(VMThread* t, Object* o);
int          monExit(VMThread* t, Object* o);    /* 0=ok 1=非法释放 */
void         monWait(VMThread* t, Object* o, int64_t ms);
void         monNotify(VMThread* t, Object* o, int all);

Object*      internString(VMThread* t, Object* s);

/* ============ interp.c ============ */
void interpRun(VMThread* t);                     /* 执行 top 帧 until 返回/异常 */
void interpInvoke(VMThread* t, Method* m, Slot* args); /* 压新帧并执行（含 native/抽象检查） */
void throwPending(VMThread* t);                  /* 触发 unwind（t->exc 已设置） */

/* 异常匹配：catch 类名是否匹配 exc 对象的类 */
int excMatches(VMThread* t, Object* exc, Class* catchClass);

/* ============ vm.c ============ */
void* vmAllocMem(size_t n);                      /* C 内存（失败 abort） */
char* vmStrdup(const char* s);
/* 修改版 UTF-8 <-> UTF-16 */
uint16_t* mutf8ToUtf16(const char* s, uint32_t len, int* outLen);
char*     utf16ToUtf8(const uint16_t* w, int len);       /* malloc */
/* 方法描述符解析 */
int  descArgSlots(const char* desc);

/* 线程辅助 */
void threadRegister(VMThread* t);
void threadUnregister(VMThread* t);
void threadSleepMs(int64_t ms);

/* 平台毫秒时钟 */
int64_t vmNowMillis(void);
int64_t vmNowNanos(void);

#endif
