/* interp.c - JVM 字节码解释器（switch-dispatch，每轮循环顶检测帧切换/异常）
 * 约定：long/double 占 2 栈槽/局部槽（JVM 规范），值存第一槽，第二槽为占位
 * float 槽存 int32 位模式；double 槽存 double 值
 * GC 安全纪律：指令中未入栈的新对象引用不跨 vmAlloc 调用；
 *             native 执行期间 t->gcDisable++；multianewarray 预 GC + noGC
 */
#include "vm_internal.h"
#include <math.h>

/* ============ 浮点位模式辅助 ============ */
static inline float slot2f(Slot s) { uint32_t b = (uint32_t)s.i; float f; memcpy(&f, &b, 4); return f; }
static inline Slot  f2slot(float f) { Slot s; uint32_t b; memcpy(&b, &f, 4); s.i = (int32_t)b; s.ref = NULL; return s; }

/* JVM 规范浮点->整型转换（NaN->0，溢出夹紧） */
static int32_t f2i_(float a) {
    if (isnan(a)) return 0;
    if (a >= 2147483520.0f) return INT32_MAX;    /* 2^31-128 以上无法精确表示 */
    if (a <= -2147483648.0f) return INT32_MIN;
    return (int32_t)a;
}
static int64_t f2l_(float a) {
    if (isnan(a)) return 0;
    if (a >= 9223372036854775808.0f) return INT64_MAX;
    if (a <= -9223372036854775808.0f) return INT64_MIN;
    return (int64_t)a;
}
static int32_t d2i_(double a) {
    if (isnan(a)) return 0;
    if (a >= 2147483647.0) return INT32_MAX;
    if (a <= -2147483648.0) return INT32_MIN;
    return (int32_t)a;
}
static int64_t d2l_(double a) {
    if (isnan(a)) return 0;
    if (a >= 9223372036854775807.0) return INT64_MAX;
    if (a <= -9223372036854775808.0) return INT64_MIN;
    return (int64_t)a;
}

/* 大端读取 */
static inline int32_t rd32(const uint8_t* p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

/* ============ 栈操作宏（帧 f / 方法 m 所在作用域内使用） ============ */
#define POP_I32()   ((int32_t)f->stack[--f->sp].i)
#define POP_I64()   (f->sp -= 2, f->stack[f->sp].i)
#define POP_F32()   slot2f(f->stack[--f->sp])
#define POP_F64()   (f->sp -= 2, f->stack[f->sp].d)
#define POP_REF()   (f->sp--, f->stack[f->sp].ref)

#define PUSH_I32(v) (f->stack[f->sp++].i = (int64_t)(int32_t)(v))
#define PUSH_I64(v) (f->stack[f->sp].i = (v), f->stack[f->sp+1] = f->stack[f->sp], f->sp += 2)
#define PUSH_F32(v) (f->stack[f->sp++] = f2slot(v))
#define PUSH_F64(v) (f->stack[f->sp].d = (v), f->stack[f->sp+1] = f->stack[f->sp], f->sp += 2)
#define PUSH_REF(v) (f->stack[f->sp++].ref = (void*)(v))

/* 读取 int16 分支偏移（先读后跳） */
#define RD_OFF16() ((int16_t)(((int16_t)code[f->pc] << 8) | (int16_t)code[f->pc+1]))

/* ============ 异常 ============ */

/* 常量池条目（Class/Fieldref/Methodref/InterfaceMethodref）对应的类名。
 * 用于 NoClassDefFoundError 的 message，便于定位游戏缺哪个 API 类。 */
static const char* cpRefClassName(Class* self, CpEntry* e) {
    if (e->tag == 9 || e->tag == 10 || e->tag == 11) {
        if (e->idx1 == 0 || e->idx1 >= (uint16_t)self->nCp) return "?";
        e = &self->cp[e->idx1];
    }
    if (e->tag != 7 || e->idx1 == 0 || e->idx1 >= (uint16_t)self->nCp) return "?";
    if (self->cp[e->idx1].tag != 1) return "?";
    char* n = self->cp[e->idx1].utf8;
    return (n && *n) ? n : "?";
}

void vmThrowObject(VMThread* t, Object* exc) {
    if (!exc || t->exc) return;
    t->exc = exc;
}

void vmThrow(VMThread* t, const char* cls, const char* msg) {
    Object* single = NULL;
    if (strcmp(cls, "java/lang/NullPointerException") == 0) single = vmG.excNPE;
    else if (strcmp(cls, "java/lang/ClassCastException") == 0) single = vmG.excCCE;
    else if (strcmp(cls, "java/lang/ArrayIndexOutOfBoundsException") == 0) single = vmG.excAIOOBE;
    else if (strcmp(cls, "java/lang/ArithmeticException") == 0) single = vmG.excArith;
    else if (strcmp(cls, "java/lang/StringIndexOutOfBoundsException") == 0) single = vmG.excSIOOBE;
    else if (strcmp(cls, "java/lang/OutOfMemoryError") == 0) single = vmG.excOOM;

    if (!single) {
        Class* c = vmFindClass(cls);
        if (!c) { vmLog("[vm] 异常类缺失 %s", cls); if (vmG.excOOM) t->exc = vmG.excOOM; return; }
        Object* o = vmNewObjectNoCtor(t, c);
        if (!o) { t->exc = vmG.excOOM; return; }
        Method* init = vmFindMethod(c, "<init>", "(Ljava/lang/String;)V");
        if (init) {
            Slot args[2];
            args[0].ref = JREF(o);
            args[1].ref = msg ? JREF(vmNewStringUTF(t, msg)) : NULL;
            vmCall(t, init, args);
            if (vmHasException(t)) vmTakeException(t);
        }
        t->exc = o;
        return;
    }
    if (msg) vmLog("[vm] 异常 %s: %s", cls, msg);
    t->exc = single;
}

int vmHasException(VMThread* t) { return t->exc != NULL; }
Object* vmTakeException(VMThread* t) { Object* e = t->exc; t->exc = NULL; return e; }
void vmClearException(VMThread* t) { t->exc = NULL; }

int excMatches(VMThread* t, Object* exc, Class* catchClass) {
    (void)t;
    if (!exc || !catchClass) return 0;
    return vmIsSubclass(exc->clazz, catchClass);
}

/* 返回 1=已跳到 handler；0=未捕获（帧已清空至哨兵，t->exc 保留） */
static int handleException(VMThread* t) {
    Object* exc = t->exc;
    while (t->top) {
        Frame* f = t->top;
        if (!f->m) break; /* 哨兵帧不 pop（vmCall 管理） */
        Method* m = f->m;
        uint32_t tp = f->thrownPc;
        int handled = 0;
        for (int i = 0; i < m->nExc; i++) {
            uint32_t start = m->exc[i * 4], end = m->exc[i * 4 + 1];
            uint32_t handler = m->exc[i * 4 + 2], catchIdx = m->exc[i * 4 + 3];
            if (tp < start || tp >= end) continue;
            if (catchIdx == 0) {
                handled = 1;
            } else {
                Class* cc = cpClass(t, &m->clazz->cp[catchIdx], m->clazz);
                if (cc && excMatches(t, exc, cc)) handled = 1;
            }
            if (handled) {
                t->exc = NULL;
                f->sp = 0;
                f->stack[f->sp++].ref = JREF(exc);
                f->pc = handler;
                return 1;
            }
        }
        t->top = f->prev;
        free(f->locals); free(f->stack); free(f);
    }
    return 0;
}

/* ============ 帧管理 ============ */

static int pushFrame(VMThread* t, Method* m) {
    Frame* f = t->top;
    Frame* nf = (Frame*)malloc(sizeof(Frame));
    if (!nf) return -1;
    int nl = m->maxLocals ? m->maxLocals : 1;
    int ns = m->maxStack ? m->maxStack : 1;
    nf->locals = (Slot*)malloc(sizeof(Slot) * nl);
    nf->stack = (Slot*)malloc(sizeof(Slot) * ns);
    if (!nf->locals || !nf->stack) { free(nf->locals); free(nf->stack); free(nf); return -1; }
    for (int i = 0; i < m->nargs; i++)
        nf->locals[i] = f->stack[f->sp - m->nargs + i];
    f->sp -= m->nargs;
    for (int i = m->nargs; i < nl; i++) { nf->locals[i].i = 0; nf->locals[i].ref = NULL; }
    nf->m = m;
    nf->sp = 0; nf->pc = 0; nf->thrownPc = 0;
    nf->prev = f;
    t->top = nf;
    return 0;
}

static void popFrame(VMThread* t, Slot retVal, int retKind) {
    Frame* f = t->top;
    t->top = f->prev;
    free(f->locals); free(f->stack); free(f);
    Frame* c = t->top;
    if (c && retKind != 'V') {
        c->stack[c->sp++] = retVal;
        if (retKind == 'J' || retKind == 'D') c->stack[c->sp++] = retVal;
    }
}

/* ============ invoke ============ */

typedef enum { IK_STATIC, IK_SPECIAL, IK_VIRTUAL, IK_INTERFACE } InvKind;

static void doInvoke(VMThread* t, uint16_t cpIdx, InvKind kind) {
    Frame* f = t->top;
    Method* caller = f->m;
    CpEntry* e = &caller->clazz->cp[cpIdx];
    Method* target = cpMethod(t, e, caller->clazz);
    if (!target) { vmThrow(t, "java/lang/NoSuchMethodError", NULL); return; }

    if (kind == IK_VIRTUAL || kind == IK_INTERFACE) {
        Object* recv = UNREF(f->stack[f->sp - target->nargs].ref);
        if (!recv) { vmThrow(t, "java/lang/NullPointerException", NULL); return; }
        if (target->vtableIdx < recv->clazz->vtableSize) {
            Method* vt = recv->clazz->vtable[target->vtableIdx];
            if (vt && strcmp(vt->name, target->name) == 0 && strcmp(vt->desc, target->desc) == 0)
                target = vt;
            else
                target = vmFindMethodImpl(recv->clazz, target->name, target->desc);
        } else {
            target = vmFindMethodImpl(recv->clazz, target->name, target->desc);
        }
        if (!target) { vmThrow(t, "java/lang/NoSuchMethodError", NULL); return; }
    }

    if (cfInitClass(t, target->clazz) != 0) {
        if (!vmHasException(t)) vmThrow(t, "java/lang/ExceptionInInitializerError", NULL);
        return;
    }

    if (target->native) {
        t->gcDisable++;
        Slot ret = target->native(t, &f->stack[f->sp - target->nargs]);
        t->gcDisable--;
        if (vmHasException(t)) return; /* 帧 unwind 会清理栈 */
        f->sp -= target->nargs;
        int rk = vmDescReturnKind(target->desc);
        if (rk != 'V') {
            f->stack[f->sp++] = ret;
            if (rk == 'J' || rk == 'D') f->stack[f->sp++] = ret;
        }
        return;
    }
    if (!target->code) { vmThrow(t, "java/lang/AbstractMethodError", NULL); return; }
    if (pushFrame(t, target) != 0) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); return; }
}

/* ============ 数组辅助 ============ */

static Class* classForNewArray(int atype) {
    static const char* names[] = { NULL, NULL, NULL, NULL, "[Z", "[C", "[F", "[D", "[B", "[S", "[I", "[J" };
    if (atype < 4 || atype > 11) return NULL;
    return vmFindArrayClass(names[atype]);
}

static int elemSizeOf(int kind) {
    switch (kind) {
        case EK_BYTE: return 1;
        case EK_CHAR: case EK_SHORT: return 2;
        case EK_INT: case EK_FLOAT: return 4;
        case EK_LONG: case EK_DOUBLE: return 8;
        default: return sizeof(Slot);
    }
}

static Object* buildMultiArray(VMThread* t, Class* arrCls, int32_t* dims, int dim) {
    int n = dims[dim];
    Object* a = heapAllocNoGC(t, arrCls, ARRAY_EXTRA_BYTES + (size_t)n * elemSizeOf(arrCls->elemKind));
    if (!a) abort();
    ArrayObject* arr = (ArrayObject*)a;
    arr->length = n;
    memset(ARRAY_DATA(arr), 0, (size_t)n * elemSizeOf(arrCls->elemKind));
    if (arrCls->elemKind == EK_OBJ) {
        Class* sub = arrCls->elemClass;
        if (sub && sub->isArray && dim < 15) {
            Slot* el = (Slot*)ARRAY_DATA(arr);
            for (int i = 0; i < n; i++)
                el[i].ref = JREF(buildMultiArray(t, sub, dims, dim + 1));
        }
    }
    return a;
}

/* 数组访问公共检查：弹出索引与数组引用并检查（值已在调用处弹出） */
#define ARR_STORE_PRE() \
    int32_t ai_ = (int32_t)f->stack[--f->sp].i; \
    Object* ao_ = UNREF(f->stack[--f->sp].ref); \
    if (!ao_) { vmThrow(t, "java/lang/NullPointerException", NULL); break; } \
    ArrayObject* arr_ = (ArrayObject*)ao_; \
    if (ai_ < 0 || ai_ >= arr_->length) { vmThrow(t, "java/lang/ArrayIndexOutOfBoundsException", NULL); break; }

#define ARR_LOAD_PRE() \
    int32_t ai_ = (int32_t)f->stack[--f->sp].i; \
    Object* ao_ = UNREF(f->stack[--f->sp].ref); \
    if (!ao_) { vmThrow(t, "java/lang/NullPointerException", NULL); break; } \
    ArrayObject* arr_ = (ArrayObject*)ao_; \
    if (ai_ < 0 || ai_ >= arr_->length) { vmThrow(t, "java/lang/ArrayIndexOutOfBoundsException", NULL); break; }

/* ============ 主解释循环 ============ */

void interpRun(VMThread* t) {
    if (!t->top) return;
    Frame* f = t->top;
    if (!f->m || !f->m->code) return;
    Method* m = f->m;
    uint8_t* code = m->code;

    for (;;) {
        /* 异常处理 */
        if (t->exc) {
            if (!handleException(t)) return; /* 未捕获，帧已清空 */
            f = t->top;
            if (!f->m || !f->m->code) return;
            m = f->m; code = m->code;
        }
        /* 帧切换检测（invoke 压帧 / return 弹帧后） */
        if (t->top != f) {
            if (!t->top) return;
            f = t->top;
            if (!f->m || !f->m->code) return;
            m = f->m; code = m->code;
        }

        uint32_t op_pc = f->pc;
        uint8_t op = code[op_pc];
        f->thrownPc = op_pc;
        f->pc = op_pc + 1;

        switch (op) {

        /* ---- 常量 ---- */
        case 0x00: break;
        case 0x01: PUSH_REF(NULL); break;
        case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08:
            PUSH_I32((int)op - 3); break;
        case 0x09: case 0x0a: PUSH_I64((int64_t)(op - 0x09)); break;
        case 0x0b: case 0x0c: case 0x0d: PUSH_F32((float)(op - 0x0b)); break;
        case 0x0e: case 0x0f: PUSH_F64((double)(op - 0x0e)); break;
        case 0x10: { int8_t v = (int8_t)code[f->pc]; f->pc++; PUSH_I32(v); break; }
        case 0x11: { int16_t v = RD_OFF16(); f->pc += 2; PUSH_I32(v); break; }
        case 0x12: case 0x13: {
            uint16_t idx = (op == 0x12) ? code[f->pc] : (uint16_t)((code[f->pc] << 8) | code[f->pc+1]);
            f->pc += (op == 0x12) ? 1 : 2;
            CpEntry* e = &m->clazz->cp[idx];
            if (e->tag == 3 || e->tag == 4) f->stack[f->sp++].i = e->ival;
            else if (e->tag == 8) {
                Object* s = cpString(t, e, m->clazz);
                if (!s) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); break; }
                PUSH_REF(JREF(s));
            } else if (e->tag == 7) {
                Class* c = cpClass(t, e, m->clazz);
                if (!c) { vmThrow(t, "java/lang/NoClassDefFoundError", cpRefClassName(m->clazz, e)); break; }
                Object* co = vmGetClassObj(t, c);
                if (!co) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); break; }
                PUSH_REF(JREF(co));
            } else vmThrow(t, "java/lang/VerifyError", "ldc");
            break; }
        case 0x14: {
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            CpEntry* e = &m->clazz->cp[idx];
            if (e->tag == 5) PUSH_I64(e->ival);
            else if (e->tag == 6) PUSH_F64(e->dval);
            else vmThrow(t, "java/lang/VerifyError", "ldc2_w");
            break; }

        /* ---- load ---- */
        case 0x15: { uint8_t n = code[f->pc]; f->pc++; f->stack[f->sp++] = f->locals[n]; break; }
        case 0x16: { uint8_t n = code[f->pc]; f->pc++; PUSH_I64(f->locals[n].i); break; }
        case 0x17: { uint8_t n = code[f->pc]; f->pc++; f->stack[f->sp++] = f->locals[n]; break; }
        case 0x18: { uint8_t n = code[f->pc]; f->pc++; PUSH_F64(f->locals[n].d); break; }
        case 0x19: { uint8_t n = code[f->pc]; f->pc++; f->stack[f->sp++].ref = f->locals[n].ref; break; }
        case 0x1a: case 0x1b: case 0x1c: case 0x1d: f->stack[f->sp++] = f->locals[op - 0x1a]; break;
        case 0x1e: case 0x1f: case 0x20: case 0x21: PUSH_I64(f->locals[op - 0x1e].i); break;
        case 0x22: case 0x23: case 0x24: case 0x25: f->stack[f->sp++] = f->locals[op - 0x22]; break;
        case 0x26: case 0x27: case 0x28: case 0x29: PUSH_F64(f->locals[op - 0x26].d); break;
        case 0x2a: case 0x2b: case 0x2c: case 0x2d: f->stack[f->sp++].ref = f->locals[op - 0x2a].ref; break;

        /* ---- 数组读（iaload=0x2e laload=0x2f faload=0x30 daload=0x31 aaload=0x32 baload=0x33 caload=0x34 saload=0x35） ---- */
        case 0x2e: { ARR_LOAD_PRE() PUSH_I32(((int32_t*)ARRAY_DATA(arr_))[ai_]); break; }
        case 0x2f: { ARR_LOAD_PRE() PUSH_I64(((int64_t*)ARRAY_DATA(arr_))[ai_]); break; }
        case 0x30: { ARR_LOAD_PRE() f->stack[f->sp++] = f2slot(*(float*)&((int32_t*)ARRAY_DATA(arr_))[ai_]); break; }
        case 0x31: { ARR_LOAD_PRE() PUSH_F64(((double*)ARRAY_DATA(arr_))[ai_]); break; }
        case 0x32: { ARR_LOAD_PRE() PUSH_REF(((Slot*)ARRAY_DATA(arr_))[ai_].ref); break; }
        case 0x33: { ARR_LOAD_PRE() PUSH_I32(((int8_t*)ARRAY_DATA(arr_))[ai_]); break; }
        case 0x34: { ARR_LOAD_PRE() PUSH_I32(((uint16_t*)ARRAY_DATA(arr_))[ai_]); break; }
        case 0x35: { ARR_LOAD_PRE() PUSH_I32(((int16_t*)ARRAY_DATA(arr_))[ai_]); break; }

        /* ---- store ---- */
        case 0x36: { uint8_t n = code[f->pc]; f->pc++; f->locals[n] = f->stack[--f->sp]; break; }
        case 0x37: { uint8_t n = code[f->pc]; f->pc++; f->sp -= 2; f->locals[n] = f->stack[f->sp]; break; }
        case 0x38: { uint8_t n = code[f->pc]; f->pc++; f->locals[n] = f->stack[--f->sp]; break; }
        case 0x39: { uint8_t n = code[f->pc]; f->pc++; f->sp -= 2; f->locals[n] = f->stack[f->sp]; break; }
        case 0x3a: { uint8_t n = code[f->pc]; f->pc++; f->locals[n].ref = f->stack[--f->sp].ref; break; }
        case 0x3b: case 0x3c: case 0x3d: case 0x3e: f->locals[op - 0x3b] = f->stack[--f->sp]; break;
        case 0x3f: case 0x40: case 0x41: case 0x42: { int n = op - 0x3f; f->sp -= 2; f->locals[n] = f->stack[f->sp]; break; }
        case 0x43: case 0x44: case 0x45: case 0x46: f->locals[op - 0x43] = f->stack[--f->sp]; break;
        case 0x47: case 0x48: case 0x49: case 0x4a: { int n = op - 0x47; f->sp -= 2; f->locals[n] = f->stack[f->sp]; break; }
        case 0x4b: case 0x4c: case 0x4d: case 0x4e: f->locals[op - 0x4b].ref = f->stack[--f->sp].ref; break;

        /* ---- 数组写（iastore=0x4f lastore=0x50 fastore=0x51 dastore=0x52 aastore=0x53 bastore=0x54 castore=0x55 sastore=0x56） ---- */
        case 0x4f: { int32_t v = POP_I32(); ARR_STORE_PRE() ((int32_t*)ARRAY_DATA(arr_))[ai_] = v; break; }
        case 0x50: { int64_t v = POP_I64(); ARR_STORE_PRE() ((int64_t*)ARRAY_DATA(arr_))[ai_] = v; break; }
        case 0x51: { int32_t v = POP_I32(); ARR_STORE_PRE() ((int32_t*)ARRAY_DATA(arr_))[ai_] = v; break; } /* float 位模式 */
        case 0x52: { double v = POP_F64(); ARR_STORE_PRE() ((double*)ARRAY_DATA(arr_))[ai_] = v; break; }
        case 0x53: { void* v = POP_REF(); ARR_STORE_PRE() ((Slot*)ARRAY_DATA(arr_))[ai_].ref = v; break; }
        case 0x54: { int8_t v = (int8_t)POP_I32(); ARR_STORE_PRE() ((int8_t*)ARRAY_DATA(arr_))[ai_] = v; break; }
        case 0x55: { uint16_t v = (uint16_t)POP_I32(); ARR_STORE_PRE() ((uint16_t*)ARRAY_DATA(arr_))[ai_] = v; break; }
        case 0x56: { int16_t v = (int16_t)POP_I32(); ARR_STORE_PRE() ((int16_t*)ARRAY_DATA(arr_))[ai_] = v; break; }

        /* ---- 栈操作 ---- */
        case 0x57: f->sp--; break;
        case 0x58: f->sp -= 2; break;
        case 0x59: f->stack[f->sp] = f->stack[f->sp-1]; f->sp++; break;
        case 0x5a: { Slot v1 = f->stack[f->sp-1];
            f->stack[f->sp] = v1; f->stack[f->sp-1] = f->stack[f->sp-2]; f->stack[f->sp-2] = v1; f->sp++; break; }
        case 0x5b: { Slot v1 = f->stack[f->sp-1];
            f->stack[f->sp] = v1; f->stack[f->sp-1] = f->stack[f->sp-2];
            f->stack[f->sp-2] = f->stack[f->sp-3]; f->stack[f->sp-3] = v1; f->sp++; break; }
        case 0x5c: { f->stack[f->sp] = f->stack[f->sp-2]; f->stack[f->sp+1] = f->stack[f->sp-1]; f->sp += 2; break; }
        case 0x5d: { Slot v1 = f->stack[f->sp-1], v2 = f->stack[f->sp-2];
            f->stack[f->sp+1] = v1; f->stack[f->sp] = v2;
            f->stack[f->sp-1] = f->stack[f->sp-3]; f->stack[f->sp-2] = v1; f->stack[f->sp-3] = v2; f->sp += 2; break; }
        case 0x5e: { Slot v1 = f->stack[f->sp-1], v2 = f->stack[f->sp-2];
            f->stack[f->sp+1] = v1; f->stack[f->sp] = v2;
            f->stack[f->sp-1] = f->stack[f->sp-3]; f->stack[f->sp-2] = f->stack[f->sp-4];
            f->stack[f->sp-3] = v2; f->stack[f->sp-4] = v1; f->sp += 2; break; }
        case 0x5f: { Slot tmp = f->stack[f->sp-1]; f->stack[f->sp-1] = f->stack[f->sp-2]; f->stack[f->sp-2] = tmp; break; }

        /* ---- 算术：int ---- */
        case 0x60: { int32_t b = POP_I32(); int32_t a = POP_I32(); PUSH_I32(a + b); break; }
        case 0x64: { int32_t b = POP_I32(); int32_t a = POP_I32(); PUSH_I32(a - b); break; }
        case 0x68: { int32_t b = POP_I32(); int32_t a = POP_I32(); PUSH_I32(a * b); break; }
        case 0x6c: { int32_t b = POP_I32(); int32_t a = POP_I32();
            if (b == 0) { vmThrow(t, "java/lang/ArithmeticException", "/ by zero"); break; }
            if (a == INT32_MIN && b == -1) { PUSH_I32(INT32_MIN); break; }
            PUSH_I32(a / b); break; }
        case 0x70: { int32_t b = POP_I32(); int32_t a = POP_I32();
            if (b == 0) { vmThrow(t, "java/lang/ArithmeticException", "/ by zero"); break; }
            if (a == INT32_MIN && b == -1) { PUSH_I32(0); break; }
            PUSH_I32(a % b); break; }
        case 0x74: { int32_t a = POP_I32(); PUSH_I32(-a); break; }

        /* ---- 算术：long ---- */
        case 0x61: { int64_t b = POP_I64(); int64_t a = POP_I64(); PUSH_I64(a + b); break; }
        case 0x65: { int64_t b = POP_I64(); int64_t a = POP_I64(); PUSH_I64(a - b); break; }
        case 0x69: { int64_t b = POP_I64(); int64_t a = POP_I64(); PUSH_I64(a * b); break; }
        case 0x6d: { int64_t b = POP_I64(); int64_t a = POP_I64();
            if (b == 0) { vmThrow(t, "java/lang/ArithmeticException", "/ by zero"); break; }
            PUSH_I64(a / b); break; }
        case 0x71: { int64_t b = POP_I64(); int64_t a = POP_I64();
            if (b == 0) { vmThrow(t, "java/lang/ArithmeticException", "/ by zero"); break; }
            PUSH_I64(a % b); break; }
        case 0x75: { int64_t a = POP_I64(); PUSH_I64(-a); break; }

        /* ---- 算术：float ---- */
        case 0x62: { float b = POP_F32(); float a = POP_F32(); PUSH_F32(a + b); break; }
        case 0x66: { float b = POP_F32(); float a = POP_F32(); PUSH_F32(a - b); break; }
        case 0x6a: { float b = POP_F32(); float a = POP_F32(); PUSH_F32(a * b); break; }
        case 0x6e: { float b = POP_F32(); float a = POP_F32(); PUSH_F32(a / b); break; }
        case 0x72: { float b = POP_F32(); float a = POP_F32(); PUSH_F32(fmodf(a, b)); break; }
        case 0x76: { float a = POP_F32(); PUSH_F32(-a); break; }

        /* ---- 算术：double ---- */
        case 0x63: { double b = POP_F64(); double a = POP_F64(); PUSH_F64(a + b); break; }
        case 0x67: { double b = POP_F64(); double a = POP_F64(); PUSH_F64(a - b); break; }
        case 0x6b: { double b = POP_F64(); double a = POP_F64(); PUSH_F64(a * b); break; }
        case 0x6f: { double b = POP_F64(); double a = POP_F64(); PUSH_F64(a / b); break; }
        case 0x73: { double b = POP_F64(); double a = POP_F64(); PUSH_F64(fmod(a, b)); break; }
        case 0x77: { double a = POP_F64(); PUSH_F64(-a); break; }

        /* ---- 移位/位运算 ---- */
        case 0x78: { int32_t s = POP_I32() & 31; int32_t a = POP_I32(); PUSH_I32(a << s); break; }
        case 0x79: { int32_t s = POP_I32() & 63; int64_t a = POP_I64(); PUSH_I64(a << s); break; }
        case 0x7a: { int32_t s = POP_I32() & 31; int32_t a = POP_I32(); PUSH_I32(a >> s); break; }
        case 0x7b: { int32_t s = POP_I32() & 63; int64_t a = POP_I64(); PUSH_I64(a >> s); break; }
        case 0x7c: { int32_t s = POP_I32() & 31; uint32_t a = (uint32_t)POP_I32(); PUSH_I32((int32_t)(a >> s)); break; }
        case 0x7d: { int32_t s = POP_I32() & 63; uint64_t a = (uint64_t)POP_I64(); PUSH_I64((int64_t)(a >> s)); break; }
        case 0x7e: { int32_t b = POP_I32(); int32_t a = POP_I32(); PUSH_I32(a & b); break; }
        case 0x7f: { int64_t b = POP_I64(); int64_t a = POP_I64(); PUSH_I64(a & b); break; }
        case 0x80: { int32_t b = POP_I32(); int32_t a = POP_I32(); PUSH_I32(a | b); break; }
        case 0x81: { int64_t b = POP_I64(); int64_t a = POP_I64(); PUSH_I64(a | b); break; }
        case 0x82: { int32_t b = POP_I32(); int32_t a = POP_I32(); PUSH_I32(a ^ b); break; }
        case 0x83: { int64_t b = POP_I64(); int64_t a = POP_I64(); PUSH_I64(a ^ b); break; }
        case 0x84: { uint8_t n = code[f->pc]; int8_t c = (int8_t)code[f->pc+1]; f->pc += 2;
            f->locals[n].i = (int64_t)((int32_t)f->locals[n].i + c); break; }

        /* ---- 转换 ---- */
        case 0x85: { int32_t a = POP_I32(); PUSH_I64((int64_t)a); break; }
        case 0x86: { int32_t a = POP_I32(); PUSH_F32((float)a); break; }
        case 0x87: { int32_t a = POP_I32(); PUSH_F64((double)a); break; }
        case 0x88: { int64_t a = POP_I64(); PUSH_I32((int32_t)a); break; }
        case 0x89: { int64_t a = POP_I64(); PUSH_F32((float)a); break; }
        case 0x8a: { int64_t a = POP_I64(); PUSH_F64((double)a); break; }
        case 0x8b: { float a = POP_F32(); PUSH_I32(f2i_(a)); break; }
        case 0x8c: { float a = POP_F32(); PUSH_I64(f2l_(a)); break; }
        case 0x8d: { float a = POP_F32(); PUSH_F64((double)a); break; }
        case 0x8e: { double a = POP_F64(); PUSH_I32(d2i_(a)); break; }
        case 0x8f: { double a = POP_F64(); PUSH_I64(d2l_(a)); break; }
        case 0x90: { double a = POP_F64(); PUSH_F32((float)a); break; }
        case 0x91: { int32_t a = POP_I32(); PUSH_I32((int8_t)a); break; }
        case 0x92: { int32_t a = POP_I32(); PUSH_I32((uint16_t)a); break; }
        case 0x93: { int32_t a = POP_I32(); PUSH_I32((int16_t)a); break; }

        /* ---- 比较 ---- */
        case 0x94: { int64_t b = POP_I64(); int64_t a = POP_I64();
            PUSH_I32((a > b) - (a < b)); break; }
        case 0x95: case 0x96: { float b = POP_F32(); float a = POP_F32(); int r;
            if (isnan(a) || isnan(b)) r = (op == 0x96) ? 1 : -1;
            else r = (a > b) - (a < b);
            PUSH_I32(r); break; }
        case 0x97: case 0x98: { double b = POP_F64(); double a = POP_F64(); int r;
            if (isnan(a) || isnan(b)) r = (op == 0x98) ? 1 : -1;
            else r = (a > b) - (a < b);
            PUSH_I32(r); break; }

        /* ---- 分支 ---- */
        case 0x99: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_I32() == 0) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0x9a: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_I32() != 0) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0x9b: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_I32() < 0) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0x9c: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_I32() >= 0) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0x9d: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_I32() > 0) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0x9e: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_I32() <= 0) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0x9f: { int16_t off = RD_OFF16(); f->pc += 2; int32_t b = POP_I32(); int32_t a = POP_I32();
            if (a == b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa0: { int16_t off = RD_OFF16(); f->pc += 2; int32_t b = POP_I32(); int32_t a = POP_I32();
            if (a != b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa1: { int16_t off = RD_OFF16(); f->pc += 2; int32_t b = POP_I32(); int32_t a = POP_I32();
            if (a < b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa2: { int16_t off = RD_OFF16(); f->pc += 2; int32_t b = POP_I32(); int32_t a = POP_I32();
            if (a >= b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa3: { int16_t off = RD_OFF16(); f->pc += 2; int32_t b = POP_I32(); int32_t a = POP_I32();
            if (a > b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa4: { int16_t off = RD_OFF16(); f->pc += 2; int32_t b = POP_I32(); int32_t a = POP_I32();
            if (a <= b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa5: { int16_t off = RD_OFF16(); f->pc += 2; void* b = POP_REF(); void* a = POP_REF();
            if (a == b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xa6: { int16_t off = RD_OFF16(); f->pc += 2; void* b = POP_REF(); void* a = POP_REF();
            if (a != b) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xc6: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_REF() == NULL) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xc7: { int16_t off = RD_OFF16(); f->pc += 2;
            if (POP_REF() != NULL) f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }

        /* ---- goto/jsr/ret ---- */
        case 0xa7: { int16_t off = RD_OFF16(); f->pc = (uint32_t)((int32_t)op_pc + off); break; }
        case 0xa8: { int16_t off = RD_OFF16(); f->pc += 2; PUSH_I32((int32_t)f->pc); f->pc = (uint32_t)((int32_t)op_pc + off); break; }
        case 0xa9: { uint8_t n = code[f->pc]; f->pc++; f->pc = (uint32_t)f->locals[n].i; break; }
        case 0xc8: { int32_t off = rd32(code + f->pc); f->pc = (uint32_t)((int32_t)op_pc + off); break; }
        case 0xc9: { int32_t off = rd32(code + f->pc); f->pc += 4; PUSH_I32((int32_t)f->pc); f->pc = (uint32_t)((int32_t)op_pc + off); break; }

        /* ---- switch ---- */
        case 0xaa: { /* tableswitch */
            uint32_t p = (op_pc + 1 + 3) & ~3u;
            int32_t def = rd32(code + p); p += 4;
            int32_t low = rd32(code + p); p += 4;
            int32_t high = rd32(code + p); p += 4;
            int32_t idx = POP_I32();
            int32_t off = def;
            if (idx >= low && idx <= high)
                off = rd32(code + p + (uint32_t)(idx - low) * 4);
            f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }
        case 0xab: { /* lookupswitch */
            uint32_t p = (op_pc + 1 + 3) & ~3u;
            int32_t def = rd32(code + p); p += 4;
            int32_t np = rd32(code + p); p += 4;
            int32_t key = POP_I32();
            int32_t off = def;
            if (np < 0) np = 0;
            for (int32_t i = 0; i < np; i++) {
                int32_t k = rd32(code + p);
                int32_t o = rd32(code + p + 4);
                if (k == key) { off = o; break; }
                p += 8;
            }
            f->pc = (uint32_t)((int32_t)op_pc + off);
            break; }

        /* ---- 返回 ---- */
        case 0xac: { Slot rv = f->stack[--f->sp]; popFrame(t, rv, 'I'); break; }
        case 0xad: { f->sp -= 2; Slot rv = f->stack[f->sp]; popFrame(t, rv, 'J'); break; }
        case 0xae: { Slot rv = f->stack[--f->sp]; popFrame(t, rv, 'F'); break; }
        case 0xaf: { f->sp -= 2; Slot rv = f->stack[f->sp]; popFrame(t, rv, 'D'); break; }
        case 0xb0: { Slot rv; rv.i = 0; rv.ref = f->stack[--f->sp].ref; popFrame(t, rv, 'L'); break; }
        case 0xb1: { Slot rv; rv.i = 0; rv.ref = NULL; popFrame(t, rv, 'V'); break; }

        /* ---- 字段 ---- */
        case 0xb2: { /* getstatic */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            CpEntry* e = &m->clazz->cp[idx];
            Field* fd = cpField(t, e, m->clazz, 1);
            if (!fd) { vmThrow(t, "java/lang/NoSuchFieldError", NULL); break; }
            if (cfInitClass(t, fd->clazz) != 0) {
                if (!vmHasException(t)) vmThrow(t, "java/lang/ExceptionInInitializerError", NULL);
                break;
            }
            if (fd->desc[0] == 'J' || fd->desc[0] == 'D') {
                Slot v = fd->clazz->statics[fd->slot];
                f->stack[f->sp] = v; f->stack[f->sp+1] = v; f->sp += 2;
            } else {
                f->stack[f->sp++] = fd->clazz->statics[fd->slot];
            }
            break; }
        case 0xb3: { /* putstatic */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            CpEntry* e = &m->clazz->cp[idx];
            Field* fd = cpField(t, e, m->clazz, 1);
            if (!fd) { vmThrow(t, "java/lang/NoSuchFieldError", NULL); break; }
            if (cfInitClass(t, fd->clazz) != 0) {
                if (!vmHasException(t)) vmThrow(t, "java/lang/ExceptionInInitializerError", NULL);
                break;
            }
            if (fd->desc[0] == 'J' || fd->desc[0] == 'D') {
                f->sp -= 2;
                fd->clazz->statics[fd->slot] = f->stack[f->sp];
            } else {
                fd->clazz->statics[fd->slot] = f->stack[--f->sp];
            }
            break; }
        case 0xb4: { /* getfield */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            CpEntry* e = &m->clazz->cp[idx];
            Field* fd = cpField(t, e, m->clazz, 0);
            if (!fd) { vmThrow(t, "java/lang/NoSuchFieldError", NULL); break; }
            Object* o = UNREF(f->stack[f->sp-1].ref);
            if (!o) { vmThrow(t, "java/lang/NullPointerException", NULL); break; }
            Slot* fl = (Slot*)(o + 1);
            if (fd->desc[0] == 'J' || fd->desc[0] == 'D') {
                Slot v = fl[fd->slot];
                f->stack[f->sp-1] = v;
                f->stack[f->sp] = v;
                f->sp++;
            } else {
                f->stack[f->sp-1] = fl[fd->slot];
            }
            break; }
        case 0xb5: { /* putfield */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            CpEntry* e = &m->clazz->cp[idx];
            Field* fd = cpField(t, e, m->clazz, 0);
            if (!fd) { vmThrow(t, "java/lang/NoSuchFieldError", NULL); break; }
            Slot v;
            if (fd->desc[0] == 'J' || fd->desc[0] == 'D') { f->sp -= 2; v = f->stack[f->sp]; }
            else v = f->stack[--f->sp];
            Object* o = UNREF(f->stack[--f->sp].ref);
            if (!o) { vmThrow(t, "java/lang/NullPointerException", NULL); break; }
            ((Slot*)(o + 1))[fd->slot] = v;
            break; }

        /* ---- invoke ---- */
        case 0xb6: { uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            doInvoke(t, idx, IK_VIRTUAL); break; }
        case 0xb7: { uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            doInvoke(t, idx, IK_SPECIAL); break; }
        case 0xb8: { uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            doInvoke(t, idx, IK_STATIC); break; }
        case 0xb9: { uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 4; /* count+0 填充 */
            doInvoke(t, idx, IK_INTERFACE); break; }

        /* ---- 对象/数组 ---- */
        case 0xbb: { /* new */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            CpEntry* e = &m->clazz->cp[idx];
            Class* c = cpClass(t, e, m->clazz);
            if (!c || c->isArray) { vmThrow(t, "java/lang/NoClassDefFoundError", c ? c->name : cpRefClassName(m->clazz, e)); break; }
            if (cfInitClass(t, c) != 0) {
                if (!vmHasException(t)) vmThrow(t, "java/lang/ExceptionInInitializerError", NULL);
                break;
            }
            Object* o = heapAlloc(t, c, sizeof(Slot) * c->instSlots);
            if (!o) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); break; }
            PUSH_REF(JREF(o));
            break; }
        case 0xbc: { /* newarray */
            uint8_t at = code[f->pc]; f->pc++;
            int32_t n = POP_I32();
            if (n < 0) { vmThrow(t, "java/lang/NegativeArraySizeException", NULL); break; }
            Class* ac = classForNewArray(at);
            if (!ac) { vmThrow(t, "java/lang/VerifyError", "newarray"); break; }
            Object* o = heapAlloc(t, ac, ARRAY_EXTRA_BYTES + (size_t)n * elemSizeOf(ac->elemKind));
            if (!o) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); break; }
            ((ArrayObject*)o)->length = n;
            PUSH_REF(JREF(o));
            break; }
        case 0xbd: { /* anewarray */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            int32_t n = POP_I32();
            if (n < 0) { vmThrow(t, "java/lang/NegativeArraySizeException", NULL); break; }
            CpEntry* e = &m->clazz->cp[idx];
            Class* ec = cpClass(t, e, m->clazz);
            if (!ec) { vmThrow(t, "java/lang/NoClassDefFoundError", cpRefClassName(m->clazz, e)); break; }
            Class* ac;
            if (ec->isArray) {
                char buf[600];
                buf[0] = '[';
                strncpy(buf + 1, ec->name, sizeof(buf) - 2);
                buf[sizeof(buf) - 1] = 0;
                ac = vmFindArrayClass(buf);
            } else {
                ac = vmArrayClassOf(ec);
            }
            if (!ac) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); break; }
            Object* o = heapAlloc(t, ac, ARRAY_EXTRA_BYTES + (size_t)n * elemSizeOf(ac->elemKind));
            if (!o) { vmThrow(t, "java/lang/OutOfMemoryError", NULL); break; }
            ((ArrayObject*)o)->length = n;
            PUSH_REF(JREF(o));
            break; }
        case 0xbe: { /* arraylength */
            Object* o = UNREF(POP_REF());
            if (!o) { vmThrow(t, "java/lang/NullPointerException", NULL); break; }
            PUSH_I32(((ArrayObject*)o)->length);
            break; }
        case 0xbf: { /* athrow */
            Object* exc = UNREF(POP_REF());
            if (!exc) { vmThrow(t, "java/lang/NullPointerException", NULL); break; }
            vmThrowObject(t, exc);
            break; }
        case 0xc0: { /* checkcast */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            Object* o = UNREF(f->stack[f->sp-1].ref);
            if (o) {
                CpEntry* e = &m->clazz->cp[idx];
                Class* c = cpClass(t, e, m->clazz);
                if (!c) { vmThrow(t, "java/lang/NoClassDefFoundError", cpRefClassName(m->clazz, e)); break; }
                if (!vmIsSubclass(o->clazz, c)) { vmThrow(t, "java/lang/ClassCastException", c->name); break; }
            }
            break; }
        case 0xc1: { /* instanceof */
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            Object* o = UNREF(POP_REF());
            int r = 0;
            if (o) {
                CpEntry* e = &m->clazz->cp[idx];
                Class* c = cpClass(t, e, m->clazz);
                if (c && vmIsSubclass(o->clazz, c)) r = 1;
            }
            PUSH_I32(r);
            break; }

        /* ---- monitor ---- */
        case 0xc2: {
            Object* o = UNREF(f->stack[f->sp-1].ref);
            if (!o) { vmThrow(t, "java/lang/NullPointerException", NULL); break; }
            f->sp--;
            monEnter(t, o);
            break; }
        case 0xc3: {
            Object* o = UNREF(f->stack[f->sp-1].ref);
            if (!o) { vmThrow(t, "java/lang/NullPointerException", NULL); break; }
            f->sp--;
            if (monExit(t, o) != 0) { vmThrow(t, "java/lang/IllegalMonitorStateException", NULL); break; }
            break; }

        /* ---- wide ---- */
        case 0xc4: {
            uint8_t wop = code[f->pc]; f->pc++;
            uint16_t widx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            switch (wop) {
            case 0x15: f->stack[f->sp++] = f->locals[widx]; break;
            case 0x16: PUSH_I64(f->locals[widx].i); break;
            case 0x17: f->stack[f->sp++] = f->locals[widx]; break;
            case 0x18: PUSH_F64(f->locals[widx].d); break;
            case 0x19: f->stack[f->sp++].ref = f->locals[widx].ref; break;
            case 0x36: f->locals[widx] = f->stack[--f->sp]; break;
            case 0x37: f->sp -= 2; f->locals[widx] = f->stack[f->sp]; break;
            case 0x38: f->locals[widx] = f->stack[--f->sp]; break;
            case 0x39: f->sp -= 2; f->locals[widx] = f->stack[f->sp]; break;
            case 0x3a: f->locals[widx].ref = f->stack[--f->sp].ref; break;
            case 0x84: { int16_t c = (int16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
                f->locals[widx].i = (int64_t)((int32_t)f->locals[widx].i + c); break; }
            default:
                vmThrow(t, "java/lang/VerifyError", "wide");
                break;
            }
            break; }

        /* ---- multianewarray ---- */
        case 0xc5: {
            uint16_t idx = (uint16_t)((code[f->pc] << 8) | code[f->pc+1]); f->pc += 2;
            uint8_t nd = code[f->pc]; f->pc++;
            if (nd == 0 || nd > 16) { vmThrow(t, "java/lang/VerifyError", "multianewarray"); break; }
            int32_t dims[16];
            for (int i = 0; i < nd; i++)
                dims[i] = (int32_t)f->stack[f->sp - nd + i].i;
            f->sp -= nd;
            int bad = 0;
            for (int i = 0; i < nd; i++)
                if (dims[i] < 0) { vmThrow(t, "java/lang/NegativeArraySizeException", NULL); bad = 1; break; }
            if (bad) break;
            CpEntry* e = &m->clazz->cp[idx];
            Class* ac = cpClass(t, e, m->clazz);
            if (!ac) { vmThrow(t, "java/lang/NoClassDefFoundError", cpRefClassName(m->clazz, e)); break; }
            heapMaybeGC(t);
            Object* o = buildMultiArray(t, ac, dims, 0);
            PUSH_REF(JREF(o));
            break; }

        default:
            vmLog("[interp] 未实现指令 0x%02x @ %s.%s%s pc=%d", op, m->clazz->name, m->name, m->desc, op_pc);
            vmThrow(t, "java/lang/UnsupportedOperationException", "opcode");
            break;
        }
    }
}

/* ============ 对外调用入口 ============ */

/* GC 安全说明：
 * - native 路径：gcDisable 包裹，C 栈上的 args 引用安全
 * - 字节码路径：先挂哨兵+真实帧（args 已拷入 locals 被 GC 保护），
 *   再 cfInitClass（<clinit> 可能触发 GC）
 */
Slot vmCall(VMThread* t, Method* m, Slot* args) {
    Slot rv; rv.i = 0; rv.ref = NULL;
    if (!m) return rv;

    if (m->native) {
        t->gcDisable++;
        rv = m->native(t, args);
        t->gcDisable--;
        return rv;
    }
    if (!m->code) { vmThrow(t, "java/lang/AbstractMethodError", NULL); return rv; }

    /* 哨兵帧（接收返回值） */
    Slot retSlots[2];
    Frame dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.m = NULL;
    dummy.locals = NULL;
    dummy.stack = retSlots;
    dummy.sp = 0;
    dummy.prev = t->top;
    t->top = &dummy;

    /* 真实帧 */
    Frame* nf = (Frame*)malloc(sizeof(Frame));
    int nl = m->maxLocals ? m->maxLocals : 1;
    int ns = m->maxStack ? m->maxStack : 1;
    Slot* locs = (Slot*)malloc(sizeof(Slot) * nl);
    Slot* stks = (Slot*)malloc(sizeof(Slot) * ns);
    if (!nf || !locs || !stks) {
        free(locs); free(stks); free(nf);
        t->top = dummy.prev;
        vmThrow(t, "java/lang/OutOfMemoryError", NULL);
        return rv;
    }
    for (int i = 0; i < m->nargs; i++) locs[i] = args[i];
    for (int i = m->nargs; i < nl; i++) { locs[i].i = 0; locs[i].ref = NULL; }
    nf->m = m;
    nf->locals = locs;
    nf->stack = stks;
    nf->sp = 0; nf->pc = 0; nf->thrownPc = 0;
    nf->prev = &dummy;
    t->top = nf;

    /* 类初始化（args 已被 locals 保护） */
    if (cfInitClass(t, m->clazz) != 0) {
        t->top = dummy.prev;
        if (!vmHasException(t)) vmThrow(t, "java/lang/ExceptionInInitializerError", NULL);
        return rv;
    }

    interpRun(t);

    if (!t->exc && dummy.sp > 0) rv = retSlots[0];
    t->top = dummy.prev;
    return rv;
}

void interpInvoke(VMThread* t, Method* m, Slot* args) {
    vmCall(t, m, args);
}

int vmCallSpecialByName(VMThread* t, Object* obj, const char* cls,
                        const char* name, const char* desc) {
    Class* c = vmFindClass(cls);
    if (!c) return -1;
    Method* m = vmFindMethod(c, name, desc);
    if (!m) return -1;
    if (m->nargs != (obj ? 1 : 0)) return -1; /* 仅支持无参方法（可含 this） */
    Slot args[2];
    if (obj) args[0].ref = JREF(obj);
    vmCall(t, m, args);
    return vmHasException(t) ? -1 : 0;
}

void throwPending(VMThread* t) {
    /* t->exc 已设置；由解释循环顶统一 unwind。 */
    (void)t;
}
