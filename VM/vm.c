/* vm.c - VM 全局状态、初始化、线程、对象/字符串工厂、java.lang natives
 * 依赖骨架类（midp.jar，javac 编译）：
 *   java/lang/String { char[] value; }
 *   java/lang/Class  { String name; }
 *   java/lang/Throwable { String message; }
 */
#include "vm_internal.h"
#include <math.h>
#include <stdarg.h>
#include <time.h>

VMState vmG;

/* zip.c 提供 */
int zipAddJar(const char* path);
void zipCloseAll(void);

/* ============ 内存工具 ============ */

void* vmAllocMem(size_t n) {
    void* p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "[vm] OOM(%zu)\n", n); abort(); }
    memset(p, 0, n ? n : 1);   /* 结构体（Class/Method/Field/CpEntry 等）依赖零初始化 */
    return p;
}

char* vmStrdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* p = (char*)vmAllocMem(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* ============ 编码转换 ============ */

uint16_t* mutf8ToUtf16(const char* s, uint32_t len, int* outLen) {
    uint16_t* w = (uint16_t*)vmAllocMem(sizeof(uint16_t) * (len ? len : 1));
    int n = 0;
    uint32_t i = 0;
    while (i < len) {
        uint8_t b = (uint8_t)s[i];
        if (b < 0x80) {
            w[n++] = b; i++;
        } else if ((b & 0xE0) == 0xC0 && i + 1 < len) {
            w[n++] = (uint16_t)(((b & 0x1F) << 6) | ((uint8_t)s[i+1] & 0x3F));
            i += 2;
        } else if ((b & 0xF0) == 0xE0 && i + 2 < len) {
            w[n++] = (uint16_t)(((b & 0x0F) << 12) | (((uint8_t)s[i+1] & 0x3F) << 6) | ((uint8_t)s[i+2] & 0x3F));
            i += 3;
        } else {
            w[n++] = 0xFFFD; i++;
        }
    }
    *outLen = n;
    return w;
}

char* utf16ToUtf8(const uint16_t* w, int len) {
    if (len < 0) len = 0;
    char* s = (char*)vmAllocMem((size_t)len * 3 + 1);
    int n = 0;
    for (int i = 0; i < len; i++) {
        uint32_t c = w[i];
        if (c < 0x80) {
            s[n++] = (char)c;
        } else if (c < 0x800) {
            s[n++] = (char)(0xC0 | (c >> 6));
            s[n++] = (char)(0x80 | (c & 63));
        } else {
            s[n++] = (char)(0xE0 | (c >> 12));
            s[n++] = (char)(0x80 | ((c >> 6) & 63));
            s[n++] = (char)(0x80 | (c & 63));
        }
    }
    s[n] = 0;
    return s;
}

/* ============ 日志 ============ */

void vmSetPlatformHooks(PlatformHooks* h) {
    if (h) vmG.hooks = *h;
}

void vmLog(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (vmG.hooks.log) {
        vmG.hooks.log(buf);
    } else {
        fputs(buf, stderr);
        fputc('\n', stderr);
    }
}

/* ============ 时钟 ============ */

int64_t vmNowMillis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int64_t vmNowNanos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

/* ============ 线程 ============ */

void threadRegister(VMThread* t) {
    pthread_mutex_lock(&vmG.threadsMu);
    t->next = vmG.threads;
    vmG.threads = t;
    pthread_mutex_unlock(&vmG.threadsMu);
}

void threadUnregister(VMThread* t) {
    pthread_mutex_lock(&vmG.threadsMu);
    VMThread** pp = &vmG.threads;
    while (*pp && *pp != t) pp = &(*pp)->next;
    if (*pp) *pp = t->next;
    pthread_mutex_unlock(&vmG.threadsMu);
    free(t);
}

VMThread* vmThreadCreate(const char* name, Object* threadObj) {
    VMThread* t = (VMThread*)calloc(1, sizeof(VMThread));
    if (!t) abort();
    strncpy(t->name, name ? name : "Java", sizeof(t->name) - 1);
    t->threadObj = threadObj;
    threadRegister(t);
    return t;
}

VMThread* vmCurrentThread(void) {
    void* self = (void*)pthread_self();
    pthread_mutex_lock(&vmG.threadsMu);
    for (VMThread* th = vmG.threads; th; th = th->next) {
        if (th->handle == self) {
            pthread_mutex_unlock(&vmG.threadsMu);
            return th;
        }
    }
    pthread_mutex_unlock(&vmG.threadsMu);
    return NULL;
}

VMThread* vmAttachCurrent(const char* name) {
    VMThread* t = vmCurrentThread();
    if (t) return t;
    t = vmThreadCreate(name, NULL);
    t->handle = (void*)pthread_self();
    t->isMain = 1;
    return t;
}

void vmDetachCurrent(void) {
    VMThread* t = vmCurrentThread();
    if (t) threadUnregister(t);
}

void threadSleepMs(int64_t ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)((ms % 1000) * 1000000);
    nanosleep(&ts, NULL);
}

typedef struct {
    VMThread* t;
} JTArg;

static void* javaThreadMain(void* p) {
    JTArg* a = (JTArg*)p;
    VMThread* t = a->t;
    free(a);
    t->handle = (void*)pthread_self();
    pthread_detach(pthread_self());
    if (t->threadObj) {
        Method* run = vmFindMethodImpl(t->threadObj->clazz, "run", "()V");
        if (run) {
            Slot args[1];
            args[0].ref = JREF(t->threadObj);
            vmCall(t, run, args);
            if (vmHasException(t)) {
                vmLog("[vm] 线程 %s 未捕获异常", t->name);
                vmTakeException(t);
            }
        }
    }
    t->dead = 1;
    threadUnregister(t);
    return NULL;
}

int vmStartJavaThread(Object* threadObj, const char* name) {
    if (!threadObj) return -1;
    VMThread* t = vmThreadCreate(name, threadObj);
    JTArg* a = (JTArg*)malloc(sizeof(JTArg));
    if (!a) return -1;
    a->t = t;
    pthread_t h;
    if (pthread_create(&h, NULL, javaThreadMain, a) != 0) {
        free(a);
        threadUnregister(t);
        return -1;
    }
    return 0;
}

/* ============ 对象工厂 ============ */

static int elemSzOf(int kind) {
    switch (kind) {
        case EK_BYTE: return 1;
        case EK_CHAR: case EK_SHORT: return 2;
        case EK_INT: case EK_FLOAT: return 4;
        case EK_LONG: case EK_DOUBLE: return 8;
        default: return sizeof(Slot);
    }
}

Object* vmNewObjectNoCtor(VMThread* t, Class* c) {
    if (!c) return NULL;
    return heapAlloc(t, c, sizeof(Slot) * c->instSlots);
}

ArrayObject* vmNewArray(VMThread* t, Class* arrayClass, int32_t length) {
    if (!arrayClass || !arrayClass->isArray) return NULL;
    if (length < 0) return NULL;
    Object* o = heapAlloc(t, arrayClass,
                          ARRAY_EXTRA_BYTES + (size_t)length * elemSzOf(arrayClass->elemKind));
    if (!o) return NULL;
    ((ArrayObject*)o)->length = length;
    return (ArrayObject*)o;
}

ArrayObject* vmNewTypeArray(VMThread* t, int elemKind, int32_t length) {
    static const char* d[] = { "[L;", "[B", "[C", "[S", "[I", "[J", "[F", "[D" };
    if (elemKind < EK_OBJ || elemKind > EK_DOUBLE) return NULL;
    /* EK_OBJ 无元素类，走 [Ljava/lang/Object; */
    Class* ac = (elemKind == EK_OBJ)
        ? vmFindArrayClass("[Ljava/lang/Object;")
        : vmFindArrayClass(d[elemKind]);
    return vmNewArray(t, ac, length);
}

ArrayObject* vmNewObjArray(VMThread* t, Class* elemClass, int32_t length) {
    if (!elemClass) return NULL;
    Class* ac;
    if (elemClass->isArray) {
        char buf[600];
        buf[0] = '[';
        strncpy(buf + 1, elemClass->name, sizeof(buf) - 2);
        buf[sizeof(buf) - 1] = 0;
        ac = vmFindArrayClass(buf);
    } else {
        ac = vmArrayClassOf(elemClass);
    }
    return vmNewArray(t, ac, length);
}

Object* vmGetClassObj(VMThread* t, Class* c) {
    if (!c) return NULL;
    if (c->classObj) return c->classObj;
    if (!vmG.clsClass || !vmG.fldClassJavaName) return NULL;
    t->gcDisable++;
    Object* co = vmNewObjectNoCtor(t, vmG.clsClass);
    if (co) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s", c->name);
        for (char* p = buf; *p; p++)
            if (*p == '/') *p = '.';
        Object* s = vmNewStringUTF(t, buf);
        if (s) ((Slot*)(co + 1))[vmG.fldClassJavaName->slot].ref = JREF(s);
        c->classObj = co;
    }
    t->gcDisable--;
    return co;
}

/* ============ 字符串 ============ */

Field* vmStringValueField(void) {
    return vmG.fldStringValue;
}

/* 整个构造过程 gcDisable 包裹：中间态引用只在 C 栈上 */
Object* vmNewStringChars(VMThread* t, const uint16_t* chars, int len) {
    if (!vmG.clsString || !vmG.fldStringValue) return NULL;
    t->gcDisable++;
    Class* cc = vmFindArrayClass("[C");
    Object* a = cc ? heapAllocNoGC(t, cc, ARRAY_EXTRA_BYTES + (size_t)(len > 0 ? len : 0) * 2) : NULL;
    Object* s = a ? heapAllocNoGC(t, vmG.clsString, sizeof(Slot) * vmG.clsString->instSlots) : NULL;
    t->gcDisable--;
    if (!a || !s) return NULL;
    ((ArrayObject*)a)->length = len;
    if (len > 0) memcpy(ARRAY_DATA((ArrayObject*)a), chars, (size_t)len * 2);
    ((Slot*)(s + 1))[vmG.fldStringValue->slot].ref = JREF(a);
    return s;
}

Object* vmNewStringUTF(VMThread* t, const char* utf8) {
    if (!utf8) return NULL;
    int n = 0;
    uint16_t* w = mutf8ToUtf16(utf8, (uint32_t)strlen(utf8), &n);
    if (!w) return NULL;
    Object* s = vmNewStringChars(t, w, n);
    free(w);
    return s;
}

static ArrayObject* strValueArr(Object* str) {
    if (!str || !vmG.fldStringValue) return NULL;
    Slot* fl = (Slot*)(str + 1);
    return (ArrayObject*)objFromRef((intptr_t)fl[vmG.fldStringValue->slot].ref);
}

char* vmStringToUTF(VMThread* t, Object* str) {
    (void)t;
    ArrayObject* a = strValueArr(str);
    if (!a) return vmStrdup("");
    return utf16ToUtf8((uint16_t*)ARRAY_DATA(a), a->length);
}

int vmStringLength(Object* str) {
    ArrayObject* a = strValueArr(str);
    return a ? a->length : 0;
}

uint16_t* vmStringChars(Object* str) {
    ArrayObject* a = strValueArr(str);
    return a ? (uint16_t*)ARRAY_DATA(a) : NULL;
}

/* ============ java.lang natives ============ */

static Slot n_objHashCode(VMThread* t, Slot* a) {
    (void)t;
    Object* o = UNREF(a[0].ref);
    Slot r;
    r.ref = NULL;
    r.i = o ? (int64_t)(int32_t)(intptr_t)o : 0;
    return r;
}

static Slot n_objGetClass(VMThread* t, Slot* a) {
    Object* o = UNREF(a[0].ref);
    if (!o) { vmThrow(t, "java/lang/NullPointerException", NULL); Slot r; r.i = 0; r.ref = NULL; return r; }
    Object* co = vmGetClassObj(t, o->clazz);
    Slot r;
    r.ref = JREF(co);
    return r;
}

static Slot n_arraycopy(VMThread* t, Slot* a) {
    Object* src = UNREF(a[0].ref);
    int32_t sp = (int32_t)a[1].i;
    Object* dst = UNREF(a[2].ref);
    int32_t dp = (int32_t)a[3].i;
    int32_t len = (int32_t)a[4].i;
    if (!src || !dst) { vmThrow(t, "java/lang/NullPointerException", NULL); goto fail; }
    {
        ArrayObject* sa = (ArrayObject*)src;
        ArrayObject* da = (ArrayObject*)dst;
        if (len < 0 || sp < 0 || dp < 0 ||
            sp + len > sa->length || dp + len > da->length) {
            vmThrow(t, "java/lang/ArrayIndexOutOfBoundsException", "arraycopy");
            goto fail;
        }
        int es = elemSzOf(sa->header.clazz->elemKind);
        memmove((uint8_t*)ARRAY_DATA(da) + (size_t)dp * es,
                (uint8_t*)ARRAY_DATA(sa) + (size_t)sp * es,
                (size_t)len * es);
    }
fail:
    {
        Slot r;
        r.i = 0;
        r.ref = NULL;
        return r;
    }
}

static Slot n_currentTimeMillis(VMThread* t, Slot* a) {
    (void)t; (void)a;
    Slot r;
    r.ref = NULL;
    r.i = vmNowMillis();
    return r;
}

static Slot n_nanoTime(VMThread* t, Slot* a) {
    (void)t; (void)a;
    Slot r;
    r.ref = NULL;
    r.i = vmNowNanos();
    return r;
}

static Slot n_getProperty(VMThread* t, Slot* a) {
    Object* key = UNREF(a[0].ref);
    Slot r;
    r.ref = NULL;
    if (!key) { vmThrow(t, "java/lang/NullPointerException", NULL); return r; }
    char* k = vmStringToUTF(t, key);
    const char* v = NULL;
    if (k) {
        if (strcmp(k, "microedition.platform") == 0) v = "PocketJ2ME/iOS";
        else if (strcmp(k, "microedition.encoding") == 0) v = "UTF-8";
        else if (strcmp(k, "microedition.configuration") == 0) v = "CLDC-1.1";
        else if (strcmp(k, "microedition.profiles") == 0) v = "MIDP-2.0";
        else if (strcmp(k, "line.separator") == 0) v = "\n";
        free(k);
    }
    if (v) {
        Object* s = vmNewStringUTF(t, v);
        r.ref = JREF(s);
    }
    return r;
}

static Slot n_exit(VMThread* t, Slot* a) {
    (void)a;
    vmLog("[vm] System.exit(%d)", (int)a[0].i);
    vmG.midletDestroyed = 1;
    if (vmG.hooks.onMidletState) vmG.hooks.onMidletState(2);
    Slot r;
    r.i = 0;
    r.ref = NULL;
    return r;
}

static Slot n_random(VMThread* t, Slot* a) {
    (void)t; (void)a;
    Slot r;
    r.ref = NULL;
    r.d = (double)rand() / ((double)RAND_MAX + 1.0);
    return r;
}

static Slot n_sqrt(VMThread* t, Slot* a) {
    (void)t;
    Slot r;
    r.ref = NULL;
    r.d = sqrt(a[0].d);
    return r;
}

static Slot n_intern(VMThread* t, Slot* a) {
    Object* s = UNREF(a[0].ref);
    Slot r;
    r.ref = NULL;
    if (!s) { vmThrow(t, "java/lang/NullPointerException", NULL); return r; }
    r.ref = JREF(internString(t, s));
    return r;
}

static Slot n_classGetName(VMThread* t, Slot* a) {
    Object* co = UNREF(a[0].ref);
    Slot r;
    r.ref = NULL;
    if (!co) { vmThrow(t, "java/lang/NullPointerException", NULL); return r; }
    ArrayObject* nameArr = NULL;
    if (vmG.fldClassJavaName) {
        Slot* fl = (Slot*)(co + 1);
        Object* nameStr = objFromRef((intptr_t)fl[vmG.fldClassJavaName->slot].ref);
        nameArr = strValueArr(nameStr);   /* name 字段是 String，需再取其 value char[] */
    }
    if (nameArr) {
        char* s = utf16ToUtf8((uint16_t*)ARRAY_DATA(nameArr), nameArr->length);
        Object* str = s ? vmNewStringUTF(t, s) : NULL;
        free(s);
        r.ref = JREF(str);
    }
    return r;
}

static Slot n_threadCurrent(VMThread* t, Slot* a) {
    (void)a;
    Slot r;
    r.ref = JREF(t->threadObj);
    return r;
}

static Slot n_threadSleep(VMThread* t, Slot* a) {
    int64_t ms = a[0].i;
    if (ms > 0) threadSleepMs(ms);
    Slot r;
    r.i = 0;
    r.ref = NULL;
    if (t->interrupted) {
        t->interrupted = 0;
        vmThrow(t, "java/lang/InterruptedException", NULL);
    }
    return r;
}

static Slot n_threadStart(VMThread* t, Slot* a) {
    Object* self = UNREF(a[0].ref);
    Slot r;
    r.i = 0;
    r.ref = NULL;
    if (!self) { vmThrow(t, "java/lang/NullPointerException", NULL); return r; }
    if (vmStartJavaThread(self, "Thread") != 0)
        vmThrow(t, "java/lang/OutOfMemoryError", "thread");
    return r;
}

static VMThread* findThreadByObj(Object* o) {
    pthread_mutex_lock(&vmG.threadsMu);
    for (VMThread* th = vmG.threads; th; th = th->next) {
        if (th->threadObj == o) {
            pthread_mutex_unlock(&vmG.threadsMu);
            return th;
        }
    }
    pthread_mutex_unlock(&vmG.threadsMu);
    return NULL;
}

static Slot n_threadInterrupt(VMThread* t, Slot* a) {
    Object* self = UNREF(a[0].ref);
    (void)t;
    VMThread* target = self ? findThreadByObj(self) : NULL;
    if (target) target->interrupted = 1;
    Slot r;
    r.i = 0;
    r.ref = NULL;
    return r;
}

static Slot n_threadIsInterrupted(VMThread* t, Slot* a) {
    Object* self = UNREF(a[0].ref);
    VMThread* target = self ? findThreadByObj(self) : NULL;
    Slot r;
    r.ref = NULL;
    r.i = target ? target->interrupted : 0;
    return r;
}

static Slot n_dtos(VMThread* t, Slot* a) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%g", a[0].d);
    Slot r;
    r.ref = JREF(vmNewStringUTF(t, buf));
    return r;
}

static Slot n_ftos(VMThread* t, Slot* a) {
    char buf[40];
    float f;
    uint32_t b = (uint32_t)a[0].i;
    memcpy(&f, &b, 4);
    snprintf(buf, sizeof(buf), "%g", (double)f);
    Slot r;
    r.ref = JREF(vmNewStringUTF(t, buf));
    return r;
}

/* ---- 浮点位模式转换（float 槽存 int32 位模式，double 槽存 double 值） ---- */
static Slot n_floatToIntBits(VMThread* t, Slot* a) {
    (void)t; Slot r; r.ref = NULL; r.i = a[0].i; return r;
}
static Slot n_intBitsToFloat(VMThread* t, Slot* a) {
    (void)t; Slot r; r.ref = NULL; r.i = a[0].i; return r;
}
static Slot n_doubleToLongBits(VMThread* t, Slot* a) {
    (void)t; Slot r; r.ref = NULL;
    double d = a[0].d; int64_t b; memcpy(&b, &d, 8); r.i = b; return r;
}
static Slot n_longBitsToDouble(VMThread* t, Slot* a) {
    (void)t; Slot r; r.ref = NULL;
    int64_t b = a[0].i; double d; memcpy(&d, &b, 8); r.d = d; return r;
}

/* ---- Math 三角/指对数族 ---- */
#define DEF_MATH1(NAME, EXPR) \
    static Slot n_##NAME(VMThread* t, Slot* a) { \
        (void)t; Slot r; r.ref = NULL; r.d = EXPR; return r; \
    }
DEF_MATH1(sin, sin(a[0].d))
DEF_MATH1(cos, cos(a[0].d))
DEF_MATH1(tan, tan(a[0].d))
DEF_MATH1(asin, asin(a[0].d))
DEF_MATH1(acos, acos(a[0].d))
DEF_MATH1(atan, atan(a[0].d))
DEF_MATH1(atan2, atan2(a[0].d, a[1].d))
DEF_MATH1(pow, pow(a[0].d, a[1].d))
DEF_MATH1(log, log(a[0].d))
DEF_MATH1(exp, exp(a[0].d))
DEF_MATH1(floor, floor(a[0].d))
DEF_MATH1(ceil, ceil(a[0].d))

static Slot n_threadIsAlive(VMThread* t, Slot* a) {
    Object* self = UNREF(a[0].ref);
    Slot r;
    r.i = 0;
    r.ref = NULL;
    if (!self) return r;
    if (t->threadObj == self) { r.i = 1; return r; }
    r.i = findThreadByObj(self) ? 1 : 0;
    return r;
}

static Slot n_classForName(VMThread* t, Slot* a) {
    Object* name = UNREF(a[0].ref);
    Slot r;
    r.i = 0;
    r.ref = NULL;
    if (!name) { vmThrow(t, "java/lang/NullPointerException", NULL); return r; }
    char* n = vmStringToUTF(t, name);
    if (!n) return r;
    for (char* p = n; *p; p++)
        if (*p == '.') *p = '/';
    Class* c = (n[0] == '[') ? vmFindArrayClass(n) : vmFindClass(n);
    if (!c) { vmThrow(t, "java/lang/ClassNotFoundException", n); free(n); return r; }
    free(n);
    r.ref = JREF(vmGetClassObj(t, c));
    return r;
}

static Slot n_print(VMThread* t, Slot* a) {
    Object* s = UNREF(a[1].ref);   /* a[0]=this(PrintStream), a[1]=String */
    if (!s) {
        fputs("null", stdout);
    } else {
        char* u = vmStringToUTF(t, s);
        if (u) { fputs(u, stdout); free(u); }
    }
    fflush(stdout);
    Slot r;
    r.i = 0;
    r.ref = NULL;
    return r;
}

static void registerCoreNatives(void) {
    vmRegisterNative("java/lang/Object", "hashCode", "()I", n_objHashCode);
    vmRegisterNative("java/lang/Object", "getClass", "()Ljava/lang/Class;", n_objGetClass);
    vmRegisterNative("java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", n_arraycopy);
    vmRegisterNative("java/lang/System", "currentTimeMillis", "()J", n_currentTimeMillis);
    vmRegisterNative("java/lang/System", "nanoTime", "()J", n_nanoTime);
    vmRegisterNative("java/lang/System", "getProperty", "(Ljava/lang/String;)Ljava/lang/String;", n_getProperty);
    vmRegisterNative("java/lang/System", "exit", "(I)V", n_exit);
    vmRegisterNative("java/lang/Math", "random", "()D", n_random);
    vmRegisterNative("java/lang/Math", "sqrt", "(D)D", n_sqrt);
    vmRegisterNative("java/lang/Math", "sin", "(D)D", n_sin);
    vmRegisterNative("java/lang/Math", "cos", "(D)D", n_cos);
    vmRegisterNative("java/lang/Math", "tan", "(D)D", n_tan);
    vmRegisterNative("java/lang/Math", "asin", "(D)D", n_asin);
    vmRegisterNative("java/lang/Math", "acos", "(D)D", n_acos);
    vmRegisterNative("java/lang/Math", "atan", "(D)D", n_atan);
    vmRegisterNative("java/lang/Math", "atan2", "(DD)D", n_atan2);
    vmRegisterNative("java/lang/Math", "pow", "(DD)D", n_pow);
    vmRegisterNative("java/lang/Math", "log", "(D)D", n_log);
    vmRegisterNative("java/lang/Math", "exp", "(D)D", n_exp);
    vmRegisterNative("java/lang/Math", "floor", "(D)D", n_floor);
    vmRegisterNative("java/lang/Math", "ceil", "(D)D", n_ceil);
    vmRegisterNative("java/lang/String", "intern", "()Ljava/lang/String;", n_intern);
    vmRegisterNative("java/lang/Class", "getName", "()Ljava/lang/String;", n_classGetName);
    vmRegisterNative("java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;", n_classForName);
    vmRegisterNative("java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", n_threadCurrent);
    vmRegisterNative("java/lang/Thread", "sleep", "(J)V", n_threadSleep);
    vmRegisterNative("java/lang/Thread", "start", "()V", n_threadStart);
    vmRegisterNative("java/lang/Thread", "interrupt", "()V", n_threadInterrupt);
    vmRegisterNative("java/lang/Thread", "isInterrupted", "()Z", n_threadIsInterrupted);
    vmRegisterNative("java/lang/Thread", "isAlive", "()Z", n_threadIsAlive);
    vmRegisterNative("java/lang/Double", "toString", "(D)Ljava/lang/String;", n_dtos);
    vmRegisterNative("java/lang/Float", "toString", "(F)Ljava/lang/String;", n_ftos);
    vmRegisterNative("java/lang/Float", "floatToIntBits", "(F)I", n_floatToIntBits);
    vmRegisterNative("java/lang/Float", "intBitsToFloat", "(I)F", n_intBitsToFloat);
    vmRegisterNative("java/lang/Double", "doubleToLongBits", "(D)J", n_doubleToLongBits);
    vmRegisterNative("java/lang/Double", "longBitsToDouble", "(J)D", n_longBitsToDouble);
    vmRegisterNative("java/io/PrintStream", "print", "(Ljava/lang/String;)V", n_print);
}

/* ============ 初始化/销毁 ============ */

int vmInit(const char* midpJarPath) {
    memset(&vmG, 0, sizeof(vmG));
    pthread_mutex_init(&vmG.threadsMu, NULL);
    pthread_mutex_init(&vmG.gcMu, NULL);
    vmG.gcThreshold = 8 * 1024 * 1024;

    if (midpJarPath && zipAddJar(midpJarPath) != 0) {
        vmLog("[vm] 内置 API jar 加载失败: %s", midpJarPath);
        return -1;
    }

    /* 注册 natives（parseClass 时 bindNatives + 注册时补绑已加载类） */
    registerCoreNatives();

    /* attach 主线程 */
    VMThread* main = vmAttachCurrent("main");
    (void)main;

    /* 核心类加载（骨架在 midp.jar 内） */
    vmG.clsObject = vmFindClass("java/lang/Object");
    if (!vmG.clsObject) { vmLog("[vm] java/lang/Object 加载失败"); return -1; }
    vmG.clsString = vmFindClass("java/lang/String");
    vmG.clsClass = vmFindClass("java/lang/Class");
    vmG.clsThread = vmFindClass("java/lang/Thread");
    vmG.clsThrowable = vmFindClass("java/lang/Throwable");
    if (!vmG.clsString || !vmG.clsClass || !vmG.clsThread || !vmG.clsThrowable) {
        vmLog("[vm] 核心类加载失败（midp.jar 不完整?）");
        return -1;
    }

    /* 关键字段 */
    vmG.fldStringValue = vmFindField(vmG.clsString, "value", "[C");
    vmG.fldClassJavaName = vmFindField(vmG.clsClass, "name", "Ljava/lang/String;");
    if (!vmG.fldStringValue || !vmG.fldClassJavaName) {
        vmLog("[vm] String.value / Class.name 字段缺失（骨架不匹配）");
        return -1;
    }

    /* 异常单例（预分配，指令中途不再分配） */
    static const struct { const char* cls; Object** slot; } singles[] = {
        { "java/lang/NullPointerException", &vmG.excNPE },
        { "java/lang/ClassCastException", &vmG.excCCE },
        { "java/lang/ArrayIndexOutOfBoundsException", &vmG.excAIOOBE },
        { "java/lang/ArithmeticException", &vmG.excArith },
        { "java/lang/StringIndexOutOfBoundsException", &vmG.excSIOOBE },
        { "java/lang/OutOfMemoryError", &vmG.excOOM },
    };
    for (size_t i = 0; i < sizeof(singles) / sizeof(singles[0]); i++) {
        Class* c = vmFindClass(singles[i].cls);
        if (!c) { vmLog("[vm] 异常类缺失 %s", singles[i].cls); return -1; }
        Object* o = vmNewObjectNoCtor(main, c);
        if (!o) return -1;
        *singles[i].slot = o;
    }

    /* 主线程的 Thread 对象 */
    main->threadObj = vmNewObjectNoCtor(main, vmG.clsThread);

    return 0;
}

void vmShutdown(void) {
    zipCloseAll();
    /* 堆/类/线程表不释放（进程即将退出） */
}
