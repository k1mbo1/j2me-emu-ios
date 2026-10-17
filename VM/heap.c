/* heap.c - 句柄表 + 对象分配 + mark-sweep GC + monitor + 字符串 intern
 * Slot.ref 存句柄索引（0=null, n -> handles[n-1]）
 * GC 保守扫描：槽值在有效句柄范围内即视为引用（误判只泄漏不崩溃）
 */
#include "vm_internal.h"
#include <time.h>

/* 本文件内后置定义 */
void internMarkRoots(void);

/* ============ 句柄表 ============ */
typedef struct HandleSlot {
    Object* o;        /* NULL=空闲 */
    int     nextFree;
} HandleSlot;

static HandleSlot* hTab = NULL;
static int hCap = 0, hCount = 0, hFreeHead = -1;
static pthread_mutex_t hMu = PTHREAD_MUTEX_INITIALIZER;

intptr_t objToRef(Object* o) {
    if (!o) return 0;
    /* 对象在分配时记录自身句柄索引于 gcm 高位? 不行 gcm 被标记用。
     * 简化：分配时在对象头后预留？——改为线性查找句柄表太慢。
     * 方案：Object.clazz 低 bit 存句柄？脏。
     * 正确方案：加字段。Object 增 selfRef。 */
    return o->selfRef;
}

Object* objFromRef(intptr_t r) {
    if (r <= 0 || r > hCap) return NULL;
    return hTab[r - 1].o;   /* 空闲槽 o=NULL → 返回 NULL（安全） */
}

static intptr_t handlePut(Object* o) {
    if (hFreeHead < 0) {
        /* 扩容 */
        int nc = hCap ? hCap * 2 : 4096;
        HandleSlot* nt = (HandleSlot*)realloc(hTab, sizeof(HandleSlot) * nc);
        if (!nt) { vmLog("[heap] 句柄表扩容失败"); abort(); }
        hTab = nt;
        for (int i = hCap; i < nc; i++) { hTab[i].o = NULL; hTab[i].nextFree = i + 1 < nc ? i + 1 : -1; }
        hFreeHead = hCap;
        hCap = nc;
    }
    int idx = hFreeHead;
    hFreeHead = hTab[idx].nextFree;
    hTab[idx].o = o;
    hTab[idx].nextFree = -1;
    hCount++;
    return (intptr_t)(idx + 1);
}

/* ============ 分配 ============ */

/* 对象大小：头 + 实例字段；数组：数组头 + 元素 */
static Object* allocRaw(VMThread* t, Class* c, size_t extra, int noGC) {
    (void)t;
    pthread_mutex_lock(&hMu);
    /* 分配字节数达阈值 → 尝试 GC（其他线程无 native GC 禁用时） */
    if (!noGC && vmG.allocatedBytes >= vmG.gcThreshold && !heapAnyThreadGcDisabled()) {
        pthread_mutex_unlock(&hMu);
        heapGC(t);
        pthread_mutex_lock(&hMu);
    }
    Object* o = (Object*)malloc(sizeof(Object) + extra);
    if (!o) {
        pthread_mutex_unlock(&hMu);
        if (noGC) { vmLog("[heap] OOM (noGC)"); abort(); }
        vmLog("[heap] OOM");
        return NULL;
    }
    memset(o, 0, sizeof(Object) + extra);
    o->clazz = c;
    o->selfRef = handlePut(o);
    vmG.allocatedBytes += sizeof(Object) + extra;
    pthread_mutex_unlock(&hMu);
    return o;
}

Object* heapAlloc(VMThread* t, Class* c, size_t extra) {
    Object* o = allocRaw(t, c, extra, 0);
    /* GC 后再试一次 */
    if (!o) {
        heapGC(t);
        o = allocRaw(t, c, extra, 1);
    }
    return o;
}

Object* heapAllocNoGC(VMThread* t, Class* c, size_t extra) {
    return allocRaw(t, c, extra, 1);
}

/* 阈值到达时收一次 GC（multianewarray 预 GC 用） */
void heapMaybeGC(VMThread* t) {
    if (vmG.allocatedBytes >= vmG.gcThreshold && !heapAnyThreadGcDisabled())
        heapGC(t);
}

/* ============ GC：mark-sweep ============ */

/* 显式标记栈 */
static Object** markStk = NULL;
static int msp = 0, mcap = 0;

static void markPush(Object* o) {
    if (!o || o->gcm) return;
    o->gcm = 1;
    if (msp == mcap) {
        mcap = mcap ? mcap * 2 : 8192;
        markStk = (Object**)realloc(markStk, sizeof(Object*) * mcap);
        if (!markStk) abort();
    }
    markStk[msp++] = o;
}

/* 槽值保守判定并标记 */
static void markSlotValue(intptr_t v) {
    if (v <= 0 || v > hCap) return;
    Object* o = hTab[v - 1].o;
    if (o) markPush(o);
}

static void markObjectGraph(void) {
    while (msp > 0) {
        Object* o = markStk[--msp];
        Class* c = o->clazz;
        if (c->isArray) {
            ArrayObject* a = (ArrayObject*)o;
            if (c->elemKind == EK_OBJ) {
                void** el = (void**)ARRAY_DATA(a);
                for (int i = 0; i < a->length; i++)
                    markSlotValue((intptr_t)el[i]);
            }
        } else if (c->instSlots > 0) {
            Slot* f = (Slot*)(o + 1);
            for (int i = 0; i < c->instSlots; i++)
                markSlotValue((intptr_t)f[i].ref);
        }
    }
}

/* 值是否像句柄（用于栈扫描） */
static int looksLikeRef(intptr_t v) {
    return v > 0 && v <= hCap && hTab[v - 1].o != NULL;
}

void heapGC(VMThread* t) {
    (void)t;
    if (vmG.gcDisabled || heapAnyThreadGcDisabled()) return;
    pthread_mutex_lock(&hMu);

    /* ---- 标记 roots ---- */
    /* 1. 线程帧（locals + 操作数栈） */
    for (VMThread* th = vmG.threads; th; th = th->next) {
        for (Frame* f = th->top; f; f = f->prev) {
            if (f->locals)
                for (uint32_t i = 0; i < f->m->maxLocals; i++)
                    if (looksLikeRef((intptr_t)f->locals[i].ref)) markPush(objFromRef((intptr_t)f->locals[i].ref));
            if (f->stack)
                for (uint32_t i = 0; i < f->sp; i++)
                    if (looksLikeRef((intptr_t)f->stack[i].ref)) markPush(objFromRef((intptr_t)f->stack[i].ref));
        }
        if (th->threadObj) markPush(th->threadObj);
        if (th->exc) markPush(th->exc);
    }
    /* 2. 类静态字段 + classObj */
    for (Class* c = vmG.classes; c; c = c->next) {
        for (int i = 0; i < c->nStaticSlots; i++)
            if (c->statics && looksLikeRef((intptr_t)c->statics[i].ref))
                markPush(objFromRef((intptr_t)c->statics[i].ref));
        if (c->classObj) markPush(c->classObj);
        /* 常量池已解析 String */
        for (int i = 1; i < c->nCp; i++)
            if (c->cp[i].tag == 8 && c->cp[i].resolved)
                markPush((Object*)c->cp[i].resolved);
    }
    /* 3. 全局根 */
    for (int i = 0; i < vmG.nGlobals; i++)
        if (vmG.globals[i]) markPush(vmG.globals[i]);
    /* 4. intern 表 */
    internMarkRoots();
    /* 5. VMState 特殊引用 */
    if (vmG.midlet) markPush(vmG.midlet);
    if (vmG.excNPE) markPush(vmG.excNPE);
    if (vmG.excCCE) markPush(vmG.excCCE);
    if (vmG.excAIOOBE) markPush(vmG.excAIOOBE);
    if (vmG.excArith) markPush(vmG.excArith);
    if (vmG.excSIOOBE) markPush(vmG.excSIOOBE);
    if (vmG.excOOM) markPush(vmG.excOOM);

    /* ---- 传递标记 ---- */
    markObjectGraph();

    /* ---- sweep ---- */
    size_t freed = 0, alive = 0;
    for (int i = 0; i < hCap; i++) {
        Object* o = hTab[i].o;
        if (!o) continue;
        if (o->gcm) { o->gcm = 0; alive++; }
        else {
            /* 释放 monitor */
            if (o->mon) {
                pthread_mutex_destroy((pthread_mutex_t*)o->mon->pmut);
                pthread_cond_destroy((pthread_cond_t*)o->mon->pcond);
                free(o->mon);
            }
            free(o);
            hTab[i].o = NULL;
            hTab[i].nextFree = hFreeHead;
            hFreeHead = i;
            freed++;
        }
    }
    hCount = (int)alive;
    vmG.allocatedBytes = 0;
    pthread_mutex_unlock(&hMu);
    vmLog("[gc] 完成: 存活 %d 释放 %d", (int)alive, (int)freed);
}

int heapAnyThreadGcDisabled(void) {
    pthread_mutex_lock(&vmG.threadsMu);
    int any = 0;
    for (VMThread* th = vmG.threads; th; th = th->next)
        if (th->gcDisable > 0) { any = 1; break; }
    pthread_mutex_unlock(&vmG.threadsMu);
    return any;
}

/* ============ Monitor ============ */

Monitor* objMonitor(Object* o) {
    if (!o) return NULL;
    if (!o->mon) {
        Monitor* m = (Monitor*)malloc(sizeof(Monitor));
        memset(m, 0, sizeof(Monitor));
        m->pmut = malloc(sizeof(pthread_mutex_t));
        m->pcond = malloc(sizeof(pthread_cond_t));
        pthread_mutex_init((pthread_mutex_t*)m->pmut, NULL);
        pthread_cond_init((pthread_cond_t*)m->pcond, NULL);
        o->mon = m;
    }
    return o->mon;
}

void monEnter(VMThread* t, Object* o) {
    Monitor* m = objMonitor(o);
    if (!m) return;
    pthread_mutex_lock((pthread_mutex_t*)m->pmut);
    if (m->owner == (void*)t) {
        m->count++;
    } else {
        while (m->owner != NULL) {
            pthread_cond_wait((pthread_cond_t*)m->pcond, (pthread_mutex_t*)m->pmut);
        }
        m->owner = (void*)t;
        m->count = 1;
    }
    pthread_mutex_unlock((pthread_mutex_t*)m->pmut);
}

int monExit(VMThread* t, Object* o) {
    Monitor* m = o ? o->mon : NULL;
    if (!m || m->owner != (void*)t) return 1;
    pthread_mutex_lock((pthread_mutex_t*)m->pmut);
    if (--m->count == 0) {
        m->owner = NULL;
        if (m->waiters > 0)
            pthread_cond_signal((pthread_cond_t*)m->pcond);
    }
    pthread_mutex_unlock((pthread_mutex_t*)m->pmut);
    return 0;
}

void monWait(VMThread* t, Object* o, int64_t ms) {
    Monitor* m = objMonitor(o);
    if (!m || m->owner != (void*)t) return; /* IllegalMonitorState，忽略 */
    pthread_mutex_lock((pthread_mutex_t*)m->pmut);
    int depth = m->count;
    m->owner = NULL; m->count = 0;
    m->waiters++;
    if (ms > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)(ms / 1000);
        ts.tv_nsec += (long)((ms % 1000) * 1000000);
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        pthread_cond_timedwait((pthread_cond_t*)m->pcond, (pthread_mutex_t*)m->pmut, &ts);
    } else {
        pthread_cond_wait((pthread_cond_t*)m->pcond, (pthread_mutex_t*)m->pmut);
    }
    m->waiters--;
    /* 重新竞争锁 */
    while (m->owner != NULL)
        pthread_cond_wait((pthread_cond_t*)m->pcond, (pthread_mutex_t*)m->pmut);
    m->owner = (void*)t; m->count = depth;
    pthread_mutex_unlock((pthread_mutex_t*)m->pmut);
}

void monNotify(VMThread* t, Object* o, int all) {
    Monitor* m = o ? o->mon : NULL;
    if (!m || m->owner != (void*)t) return;
    pthread_mutex_lock((pthread_mutex_t*)m->pmut);
    if (all) pthread_cond_broadcast((pthread_cond_t*)m->pcond);
    else pthread_cond_signal((pthread_cond_t*)m->pcond);
    pthread_mutex_unlock((pthread_mutex_t*)m->pmut);
}

/* ============ 字符串 intern ============ */
typedef struct InternEntry {
    Object* s;
    struct InternEntry* next;
} InternEntry;

static InternEntry* internTab[4096];

void internMarkRoots(void) {
    for (int i = 0; i < 4096; i++)
        for (InternEntry* e = internTab[i]; e; e = e->next)
            markPush(e->s);
}

static uint32_t strHash(Object* s) {
    /* String.value char[] 哈希（Java 规范） */
    extern Field* vmStringValueField(void);
    Field* vf = vmStringValueField();
    if (!vf) return 0;
    Slot* f = (Slot*)(s + 1);
    ArrayObject* ch = (ArrayObject*)objFromRef((intptr_t)f[vf->slot].ref);
    if (!ch) return 0;
    uint16_t* w = (uint16_t*)ARRAY_DATA(ch);
    uint32_t h = 0;
    for (int i = 0; i < ch->length; i++) h = 31 * h + w[i];
    return h;
}

static int strEq(Object* a, Object* b) {
    extern Field* vmStringValueField(void);
    Field* vf = vmStringValueField();
    if (!vf) return a == b;
    Slot* fa = (Slot*)(a + 1), *fb = (Slot*)(b + 1);
    ArrayObject* ca = (ArrayObject*)objFromRef((intptr_t)fa[vf->slot].ref);
    ArrayObject* cb = (ArrayObject*)objFromRef((intptr_t)fb[vf->slot].ref);
    if (ca == cb) return 1;
    if (!ca || !cb || ca->length != cb->length) return 0;
    uint16_t* wa = (uint16_t*)ARRAY_DATA(ca);
    uint16_t* wb = (uint16_t*)ARRAY_DATA(cb);
    return memcmp(wa, wb, sizeof(uint16_t) * ca->length) == 0;
}

Object* internString(VMThread* t, Object* s) {
    if (!s) return NULL;
    uint32_t h = strHash(s);
    uint32_t b = h & 4095;
    for (InternEntry* e = internTab[b]; e; e = e->next) {
        if (strEq(e->s, s)) return e->s;
    }
    InternEntry* e = (InternEntry*)vmAllocMem(sizeof(InternEntry));
    e->s = s;
    e->next = internTab[b];
    internTab[b] = e;
    return s;
}

/* ============ 全局根 ============ */

void vmAddGlobalRef(Object* o) {
    if (!o) return;
    if (vmG.nGlobals == vmG.capGlobals) {
        vmG.capGlobals = vmG.capGlobals * 2 + 16;
        vmG.globals = (Object**)realloc(vmG.globals, sizeof(Object*) * vmG.capGlobals);
    }
    vmG.globals[vmG.nGlobals++] = o;
}

void vmRemoveGlobalRef(Object* o) {
    for (int i = 0; i < vmG.nGlobals; i++) {
        if (vmG.globals[i] == o) {
            vmG.globals[i] = vmG.globals[--vmG.nGlobals];
            return;
        }
    }
}

void vmGCDisable(void) { vmG.gcDisabled++; }
void vmGCEnable(void) { if (vmG.gcDisabled) vmG.gcDisabled--; }
void vmGCNow(void) { heapGC(NULL); }
