/* vm.h - J2ME 虚拟机核心公共头文件
 * 纯 C 字节码解释器（无 JIT，符合 iOS W^X 限制）
 * mark-sweep GC（不移动对象，指针直接引用）
 */
#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 前置声明 ============ */
typedef struct Class Class;
typedef struct Method Method;
typedef struct Field Field;
typedef struct Object Object;
typedef struct ArrayObject ArrayObject;
typedef struct Frame Frame;
typedef struct VMThread VMThread;
typedef struct Monitor Monitor;

/* ============ 栈槽：所有 Java 值的统一存储 ============ */
typedef union Slot {
    int64_t i;      /* boolean/byte/char/short/int/long + float 位模式(低32位) */
    double  d;      /* double */
    void*   ref;    /* Object* / ArrayObject* / null */
} Slot;

/* ============ 数组元素类型 ============ */
enum {
    EK_OBJ = 0, EK_BYTE, EK_CHAR, EK_SHORT, EK_INT, EK_LONG, EK_FLOAT, EK_DOUBLE
};
/* newarray 指令 atype: 4=Z 5=C 6=F 7=D 8=B 9=S 10=I 11=J */

/* ============ 对象布局 ============
 * Slot.ref 存句柄索引（intptr_t，0=null，n 表示 handles[n-1]）
 * 句柄表间接引用：GC 安全（保守值域判定，误判仅泄漏不崩溃）
 */
struct Object {
    Class*      clazz;      /* 类（含数组伪类） */
    uint32_t    gcm;        /* GC 标记 */
    Monitor*    mon;        /* 监视器（懒创建） */
    intptr_t    selfRef;    /* 自身句柄索引（分配时记录） */
    /* 实例字段 Slot[] 紧随其后 */
};

struct ArrayObject {
    Object      header;     /* clazz 指向数组伪类 */
    int32_t     length;
    /* 元素数据紧随其后，按 elemKind 排布 */
};

/* 数组元素访问（elemKind 已知时） */
#define ARRAY_DATA(a) ((void*)((uint8_t*)(a) + sizeof(ArrayObject)))
/* 数组分配时 Object 头之后的额外字节数（length 字段 + 对齐填充，勿用 sizeof(int32_t)！） */
#define ARRAY_EXTRA_BYTES (sizeof(ArrayObject) - sizeof(Object))

/* ============ 句柄（Slot.ref <-> Object*） ============ */
#define JREF(o)     ((void*)(intptr_t)objToRef(o))     /* Object* -> 槽值 */
#define UNREF(r)    objFromRef((intptr_t)(r))          /* 槽值 -> Object* */
intptr_t   objToRef(Object* o);     /* NULL -> 0；对象 -> 句柄索引+1 */
Object*    objFromRef(intptr_t r);  /* 0 -> NULL；非法/空闲 -> NULL */

/* ============ 常量池 ============ */
typedef struct CpEntry {
    uint8_t  tag;           /* 1..12 */
    uint16_t idx1;          /* Class->name / Ref->class / NameAndType->name / String->utf8 */
    uint16_t idx2;          /* Ref->nameAndType / NameAndType->desc */
    uint32_t utf8Len;
    char*    utf8;          /* tag=1 修改版 UTF-8（NUL 结尾副本） */
    int64_t  ival;          /* Integer/Long / Float 位模式(低32) */
    double   dval;          /* Double / Float 直接值 */
    void*    resolved;      /* 已解析缓存: Class / Field / Method / String 对象 */
} CpEntry;

/* ============ 字段 ============ */
struct Field {
    Class*  clazz;
    uint16_t access;
    char*   name;
    char*   desc;
    uint16_t slot;          /* 实例: 对象内 Slot 下标 / 静态: 类 statics 下标 */
};

/* ============ 方法 ============ */
typedef Slot (*NativeFn)(VMThread* t, Slot* args);

struct Method {
    Class*   clazz;
    uint16_t access;
    char*    name;
    char*    desc;
    /* 字节码方法 */
    uint8_t* code;
    uint32_t codeLen;
    uint16_t maxStack, maxLocals;
    /* 异常表 */
    uint16_t nExc;
    uint32_t* exc;          /* 4 个 u32 一项: start,end,handler,catchTypeIdx */
    uint16_t nargs;         /* 参数 Slot 数（实例方法含 this；long/double 占 2） */
    uint16_t vtableIdx;
    NativeFn native;        /* ACC_NATIVE 时非空 */
    Method*  next;          /* 类内链 */
};

/* ============ 类 ============ */
struct Class {
    char*   name;
    uint16_t access;
    Class*  super;
    /* 成员 */
    Field*  fields;  int nFields;
    Method* methods; int nMethods;
    /* 字段布局 */
    int     instSlots;          /* 实例字段 Slot 总数（含父类） */
    Slot*   statics; int nStaticSlots;
    /* vtable（virtual 方法分派） */
    Method** vtable; int vtableSize;
    /* 常量池 */
    CpEntry* cp; int nCp;
    /* 接口 */
    Class** ifaces; int nIfaces;
    /* 数组伪类 */
    uint8_t isArray, isInterface, elemKind;
    Class*  elemClass;          /* 对象数组的元素类 */
    /* 状态 */
    uint8_t state;              /* 0=已加载 1=已初始化 */
    Object* classObj;           /* java/lang/Class 实例 */
    Class*  next;               /* 全局类链 */
};

/* ============ 帧 ============ */
struct Frame {
    Method* m;
    Slot*   locals;
    Slot*   stack;
    uint32_t sp, pc;
    uint32_t thrownPc;      /* 当前指令 pc（异常表匹配用） */
    Frame*  prev;
};

/* ============ 线程 ============ */
struct VMThread {
    void*    handle;            /* 平台线程句柄 */
    Frame*   top;               /* 当前帧链 */
    Object*  threadObj;         /* java/lang/Thread 实例 */
    char     name[64];
    int      isMain;            /* attach 的调用者线程 */
    int      interrupted;
    int      dead;
    int      gcDisable;         /* native 方法执行期间禁 GC */
    Object*  exc;               /* 未捕获异常（ unwind 到底后暂存） */
    VMThread* next;
};

/* ============ Monitor ============ */
struct Monitor {
    void*    owner;             /* VMThread* */
    int      count;
    int      waiters;
    /* 平台内部（mutex/cond）由 heap.c 定义 */
    void*    pmut;
    void*    pcond;
};

/* ============ VM 公共 API ============ */

/* 初始化/销毁。midpJar: 内置 API jar 路径。返回 0 成功 */
int  vmInit(const char* midpJarPath);
void vmShutdown(void);

/* 添加游戏 jar 到类路径（在 vmInit 之后、启动 MIDlet 之前） */
int  vmAddJar(const char* path);

/* 类查找（未加载则尝试加载，二进制名如 "java/lang/String"） */
Class* vmFindClass(const char* name);
/* 数组类（"[I"、"[Ljava/lang/String;"） */
Class* vmFindArrayClass(const char* desc);

Method* vmFindMethod(Class* c, const char* name, const char* desc);
Method* vmFindMethodImpl(Class* start, const char* name, const char* desc); /* 沿继承链查找 */
Field*  vmFindField(Class* c, const char* name, const char* desc);

/* 线程 */
VMThread* vmThreadCreate(const char* name, Object* threadObj); /* 新 VMThread（挂到线程表） */
VMThread* vmAttachCurrent(const char* name);   /* 将当前 C 线程 attach 成 VM 线程 */
void      vmDetachCurrent(void);               /* detach（attach 创建的） */
VMThread* vmCurrentThread(void);
/* 启动 java/lang/Thread（native start 调用）: 在新平台线程里跑 threadObj.run() */
int vmStartJavaThread(Object* threadObj, const char* name);

/* 调用：同步执行方法（解释或 native）。args 已按 desc 摆好（含 this）。 */
Slot vmCall(VMThread* t, Method* m, Slot* args);
/* 便捷：按名字调用（如 MIDlet.startApp） */
int  vmCallSpecialByName(VMThread* t, Object* obj, const char* cls,
                         const char* name, const char* desc);

/* 异常状态 */
int     vmHasException(VMThread* t);
Object* vmTakeException(VMThread* t);
void    vmClearException(VMThread* t);
/* 抛出内置异常（NPE 等单例，或加载类 new 实例）。msg 可为 NULL */
void vmThrow(VMThread* t, const char* cls, const char* msg);
/* 抛出已有异常对象 */
void vmThrowObject(VMThread* t, Object* exc);

/* native 注册（Bridge 层用）。cls 为二进制类名 */
void vmRegisterNative(const char* cls, const char* name, const char* desc, NativeFn fn);

/* 对象/数组创建（native 层用；不调用构造器） */
Object*      vmNewObjectNoCtor(VMThread* t, Class* c);
ArrayObject* vmNewArray(VMThread* t, Class* arrayClass, int32_t length);
ArrayObject* vmNewTypeArray(VMThread* t, int elemKind, int32_t length);
ArrayObject* vmNewObjArray(VMThread* t, Class* elemClass, int32_t length);

/* Class 实例（懒创建并缓存于 c->classObj） */
Object* vmGetClassObj(VMThread* t, Class* c);

/* 字符串 */
Object* vmNewStringUTF(VMThread* t, const char* utf8);      /* C UTF-8 -> String */
Object* vmNewStringChars(VMThread* t, const uint16_t* chars, int len);
char*   vmStringToUTF(VMThread* t, Object* str);            /* String -> malloc 的 UTF-8 */
int     vmStringLength(Object* str);
uint16_t* vmStringChars(Object* str);                       /* 直接指向内部 char[]（勿 GC 期间长期持有） */

/* 全局引用（Bridge 持久引用，GC root） */
void vmAddGlobalRef(Object* o);
void vmRemoveGlobalRef(Object* o);

/* GC */
void vmGCDisable(void);
void vmGCEnable(void);
void vmGCNow(void);

/* 工具 */
int  vmIsSubclass(Class* sub, Class* sup);       /* sub 是否 sup 的子类/同一类/实现 */
Class* vmArrayClassOf(Class* elem);              /* 元素类 -> "[Lxx;" 伪类 */
int  vmDescArgSlots(const char* desc);           /* 方法描述符参数 Slot 数 */
int  vmDescReturnKind(const char* desc);         /* 'V' 'I' 'J' 'F' 'D' 'L' '[' */

/* 平台注入（Bridge 实现） */
typedef struct PlatformHooks {
    /* MIDlet 生命周期（vmStartMidlet 回调） */
    void (*onMidletState)(int state);            /* 0=start 1=pause 2=destroy */
    /* 日志 */
    void (*log)(const char* msg);
} PlatformHooks;
void vmSetPlatformHooks(PlatformHooks* h);
void vmLog(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* VM_H */
