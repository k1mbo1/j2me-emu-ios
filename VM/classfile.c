/* classfile.c - .class 文件解析 + 类加载链接
 * 约定：float 在槽中以 int32 位模式存储；double 以 double 直接值存储
 */
#include "vm_internal.h"

/* zip.c 提供 */
int zipAddJar(const char* path);
void zipCloseAll(void);
uint8_t* zipReadClass(const char* name, size_t* outLen);
uint8_t* zipReadFile(const char* jarPath, const char* entryName, size_t* outLen);

/* ============ 读取游标 ============ */
typedef struct {
    const uint8_t* p;
    size_t left;
    const char* cls;
} Rd;

static uint8_t  ru8(Rd* r)  { if (r->left < 1) { r->left = 0; return 0; } r->left--; return *r->p++; }
static uint16_t ru16(Rd* r) { if (r->left < 2) { r->left = 0; return 0; } uint16_t v = (uint16_t)((r->p[0] << 8) | r->p[1]); r->p += 2; r->left -= 2; return v; }
static uint32_t ru32(Rd* r) { if (r->left < 4) { r->left = 0; return 0; } uint32_t v = ((uint32_t)r->p[0] << 24) | ((uint32_t)r->p[1] << 16) | ((uint32_t)r->p[2] << 8) | r->p[3]; r->p += 4; r->left -= 4; return v; }
static uint64_t ru64(Rd* r) { uint64_t hi = ru32(r), lo = ru32(r); return (hi << 32) | lo; }

static char* cpUtf8Str(CpEntry* cp, int n, uint16_t idx, const char* cls) {
    if (idx == 0 || idx >= n || cp[idx].tag != 1) {
        vmLog("[class] 常量池索引无效 (%d) @ %s", idx, cls);
        return NULL;
    }
    return cp[idx].utf8;
}

/* ============ 方法描述符 ============ */
int descArgSlots(const char* desc) {
    int n = 0; const char* p = desc;
    if (*p != '(') return 0;
    p++;
    while (*p && *p != ')') {
        if (*p == 'J' || *p == 'D') { n += 2; p++; }
        else if (*p == 'L') { n++; while (*p && *p != ';') p++; p++; }
        else if (*p == '[') { n++; while (*p == '[') p++; if (*p == 'L') { while (*p && *p != ';') p++; p++; } else p++; }
        else { n++; p++; }
    }
    return n;
}

int vmDescArgSlots(const char* desc) { return descArgSlots(desc); }

int vmDescReturnKind(const char* desc) {
    const char* p = desc;
    while (*p && *p != ')') p++;
    if (*p != ')') return 'V';
    p++;
    switch (*p) {
        case 'V': return 'V';
        case 'J': return 'J';
        case 'D': return 'D';
        case 'F': return 'F';
        case 'L': case '[': return 'L';
        default:  return 'I';
    }
}

static int descSlotWidth(char c) { return (c == 'J' || c == 'D') ? 2 : 1; }

/* ============ 访问标志 ============ */
#define ACC_PUBLIC     0x0001
#define ACC_PRIVATE    0x0002
#define ACC_PROTECTED  0x0004
#define ACC_STATIC     0x0008
#define ACC_FINAL      0x0010
#define ACC_SYNCHRONIZED 0x0020
#define ACC_SUPER      0x0020
#define ACC_NATIVE     0x0100
#define ACC_INTERFACE  0x0200
#define ACC_ABSTRACT   0x0400

/* ============ 常量池解析缓存 ============ */

Class* cpClass(VMThread* t, CpEntry* e, Class* self) {
    (void)t;
    /* 兼容 Fieldref(9)/Methodref(10)/InterfaceMethodref(11) 入参：
     * Ref 条目的 idx1 指向 Class 条目，需再解一层取 Utf8 */
    if (e->tag == 9 || e->tag == 10 || e->tag == 11) {
        if (e->idx1 == 0 || e->idx1 >= (uint16_t)self->nCp) return NULL;
        e = &self->cp[e->idx1];
    }
    if (e->resolved) return (Class*)e->resolved;
    if (e->tag != 7) return NULL;
    char* name = cpUtf8Str(self->cp, self->nCp, e->idx1, self->name);
    if (!name) return NULL;
    Class* c = (name[0] == '[') ? vmFindArrayClass(name) : vmFindClass(name);
    e->resolved = c;
    return c;
}

Field* cpField(VMThread* t, CpEntry* e, Class* self, int isStatic) {
    (void)t;
    if (e->resolved) return (Field*)e->resolved;
    Class* owner = cpClass(t, e, self);
    if (!owner) return NULL;
    CpEntry* nt = &self->cp[e->idx2];
    char* fname = cpUtf8Str(self->cp, self->nCp, nt->idx1, self->name);
    char* fdesc = cpUtf8Str(self->cp, self->nCp, nt->idx2, self->name);
    if (!fname || !fdesc) return NULL;
    for (Class* c = owner; c; c = c->super) {
        for (int i = 0; i < c->nFields; i++) {
            Field* f = &c->fields[i];
            int st = (f->access & ACC_STATIC) != 0;
            if (st == isStatic && strcmp(f->name, fname) == 0 && strcmp(f->desc, fdesc) == 0) {
                e->resolved = f;
                return f;
            }
        }
        if (isStatic) break;
    }
    if (isStatic && owner->ifaces) {
        for (int i = 0; i < owner->nIfaces; i++) {
            Class* c = owner->ifaces[i];
            for (int k = 0; k < c->nFields; k++) {
                Field* f = &c->fields[k];
                if ((f->access & ACC_STATIC) && strcmp(f->name, fname) == 0 && strcmp(f->desc, fdesc) == 0) {
                    e->resolved = f;
                    return f;
                }
            }
        }
    }
    vmLog("[class] 字段未找到 %s.%s %s", owner->name, fname, fdesc);
    return NULL;
}

Method* vmFindMethodImpl(Class* start, const char* name, const char* desc) {
    if (!start) return NULL;
    for (Class* c = start; c; c = c->super) {
        for (int i = 0; i < c->nMethods; i++) {
            Method* m = &c->methods[i];
            if (strcmp(m->name, name) == 0 && strcmp(m->desc, desc) == 0) return m;
        }
    }
    return NULL;
}

Method* vmFindMethod(Class* c, const char* name, const char* desc) {
    return vmFindMethodImpl(c, name, desc);
}

Field* vmFindField(Class* c, const char* name, const char* desc) {
    for (Class* k = c; k; k = k->super) {
        for (int i = 0; i < k->nFields; i++) {
            if (strcmp(k->fields[i].name, name) == 0 && strcmp(k->fields[i].desc, desc) == 0)
                return &k->fields[i];
        }
    }
    return NULL;
}

/* String 常量解析：mutf8 -> String -> intern -> 缓存 */
Object* cpString(VMThread* t, CpEntry* e, Class* self) {
    if (e->resolved) return (Object*)e->resolved;
    char* utf8 = cpUtf8Str(self->cp, self->nCp, e->idx1, self->name);
    if (!utf8) return NULL;
    int n = 0;
    uint16_t* w = mutf8ToUtf16(utf8, (uint32_t)strlen(utf8), &n);
    if (!w) return NULL;
    Object* s = vmNewStringChars(t, w, n);
    free(w);
    if (!s) return NULL;
    s = internString(t, s);
    e->resolved = s;
    return s;
}

/* 接口层级递归查找（接口可 extends 接口，任意深度） */
static Method* findIfaceMethodDeep(Class* iface, const char* name, const char* desc) {
    if (!iface) return NULL;
    Method* m = vmFindMethodImpl(iface, name, desc);
    if (m) return m;
    if (iface->ifaces) {
        for (int i = 0; i < iface->nIfaces; i++) {
            m = findIfaceMethodDeep(iface->ifaces[i], name, desc);
            if (m) return m;
        }
    }
    return NULL;
}

Method* cpMethod(VMThread* t, CpEntry* e, Class* self) {
    (void)t;
    if (e->resolved) return (Method*)e->resolved;
    Class* owner = cpClass(t, e, self);
    if (!owner) return NULL;
    CpEntry* nt = &self->cp[e->idx2];
    char* mname = cpUtf8Str(self->cp, self->nCp, nt->idx1, self->name);
    char* mdesc = cpUtf8Str(self->cp, self->nCp, nt->idx2, self->name);
    if (!mname || !mdesc) return NULL;
    Method* m = vmFindMethodImpl(owner, mname, mdesc);
    if (!m && owner->ifaces) {
        for (int i = 0; i < owner->nIfaces && !m; i++)
            m = findIfaceMethodDeep(owner->ifaces[i], mname, mdesc);
    }
    if (!m && vmG.clsObject && owner != vmG.clsObject)
        m = vmFindMethodImpl(vmG.clsObject, mname, mdesc);
    if (m) e->resolved = m;
    else vmLog("[class] 方法未找到 %s.%s %s", owner->name, mname, mdesc);
    return m;
}

/* ============ native 注册与绑定 ============ */
typedef struct NativeEntry {
    char cls[128], name[96], desc[128];
    NativeFn fn;
    struct NativeEntry* next;
} NativeEntry;

void vmRegisterNative(const char* cls, const char* name, const char* desc, NativeFn fn) {
    NativeEntry* n = (NativeEntry*)vmAllocMem(sizeof(NativeEntry));
    strncpy(n->cls, cls, sizeof(n->cls) - 1);
    strncpy(n->name, name, sizeof(n->name) - 1);
    strncpy(n->desc, desc, sizeof(n->desc) - 1);
    n->fn = fn;
    n->next = (NativeEntry*)vmG.natives;
    vmG.natives = n;
    /* 类已加载则立即绑定 */
    for (Class* c = vmG.classes; c; c = c->next) {
        if (strcmp(c->name, cls) == 0) {
            for (int i = 0; i < c->nMethods; i++) {
                Method* m = &c->methods[i];
                if (strcmp(m->name, name) == 0 && strcmp(m->desc, desc) == 0)
                    m->native = fn;
            }
        }
    }
}

static void bindNatives(Class* c) {
    for (NativeEntry* n = (NativeEntry*)vmG.natives; n; n = n->next) {
        if (strcmp(n->cls, c->name) != 0) continue;
        for (int i = 0; i < c->nMethods; i++) {
            Method* m = &c->methods[i];
            if (strcmp(m->name, n->name) == 0 && strcmp(m->desc, n->desc) == 0)
                m->native = n->fn;
        }
    }
}

/* ============ .class 解析 ============ */

static Class* parseClass(const uint8_t* data, size_t len) {
    Rd r = { data, len, "?" };
    if (ru32(&r) != 0xCAFEBABE) { vmLog("[class] magic 错误"); return NULL; }
    ru16(&r); ru16(&r);

    /* 常量池 */
    uint16_t nCp = ru16(&r);
    CpEntry* cp = (CpEntry*)vmAllocMem(sizeof(CpEntry) * nCp);
    memset(cp, 0, sizeof(CpEntry) * nCp);
    for (int i = 1; i < nCp; i++) {
        uint8_t tag = ru8(&r);
        cp[i].tag = tag;
        switch (tag) {
        case 1: {
            uint16_t l = ru16(&r);
            char* s = (char*)vmAllocMem((size_t)l + 1);
            for (int k = 0; k < l; k++) s[k] = (char)ru8(&r);
            s[l] = 0;
            cp[i].utf8 = s; cp[i].utf8Len = l;
            break;
        }
        case 3: cp[i].ival = (int32_t)ru32(&r); break;           /* Integer */
        case 4: cp[i].ival = (int32_t)ru32(&r); break;           /* Float 位模式 */
        case 5: cp[i].ival = (int64_t)ru64(&r); i++; break;      /* Long（占双槽） */
        case 6: { uint64_t bits = ru64(&r); memcpy(&cp[i].dval, &bits, 8); i++; break; } /* Double */
        case 7: case 8: cp[i].idx1 = ru16(&r); break;
        case 9: case 10: case 11:
        case 12: cp[i].idx1 = ru16(&r); cp[i].idx2 = ru16(&r); break;
        default:
            vmLog("[class] 常量池 tag=%d 无效", tag);
            return NULL;
        }
    }

    uint16_t access = ru16(&r);
    uint16_t thisIdx = ru16(&r);
    uint16_t superIdx = ru16(&r);
    if (thisIdx == 0 || thisIdx >= nCp || cp[thisIdx].tag != 7) {
        vmLog("[class] this_class 索引无效 (%d)", thisIdx);
        return NULL;
    }
    char* name = cpUtf8Str(cp, nCp, cp[thisIdx].idx1, "?");
    if (!name) return NULL;

    Class* c = (Class*)vmAllocMem(sizeof(Class));
    c->name = vmStrdup(name);
    c->access = access;
    c->cp = cp; c->nCp = nCp;
    c->isInterface = (access & ACC_INTERFACE) ? 1 : 0;
    r.cls = c->name;

    /* 接口（暂存 cp 索引，链接时替换为 Class*） */
    uint16_t nIf = ru16(&r);
    c->ifaces = (Class**)vmAllocMem(sizeof(Class*) * (nIf ? nIf : 1));
    for (int i = 0; i < nIf; i++)
        c->ifaces[i] = (Class*)(intptr_t)ru16(&r);
    c->nIfaces = nIf;

    /* 字段：先收集，slot 延后统一计算 */
    uint16_t nF = ru16(&r);
    c->fields = (Field*)vmAllocMem(sizeof(Field) * (nF ? nF : 1));
    c->nFields = nF;
    int* cvIdx = (int*)vmAllocMem(sizeof(int) * (nF ? nF : 1)); /* ConstantValue cp 索引，-1 无 */
    for (int i = 0; i < nF; i++) {
        Field* f = &c->fields[i];
        f->clazz = c;
        f->access = ru16(&r);
        char* fname = cpUtf8Str(cp, nCp, ru16(&r), c->name);
        char* fdesc = cpUtf8Str(cp, nCp, ru16(&r), c->name);
        if (!fname || !fdesc) return NULL;
        f->name = vmStrdup(fname); f->desc = vmStrdup(fdesc);
        cvIdx[i] = -1;
        uint16_t nAttrs = ru16(&r);
        for (int a = 0; a < nAttrs; a++) {
            char* an = cpUtf8Str(cp, nCp, ru16(&r), c->name);
            uint32_t alen = ru32(&r);
            if (an && strcmp(an, "ConstantValue") == 0 && alen == 2) {
                cvIdx[i] = ru16(&r);
            } else {
                r.p += alen; r.left -= alen;
            }
        }
    }

    /* 静态字段 slot 布局 + ConstantValue */
    int ss = 0;
    for (int i = 0; i < nF; i++) {
        Field* f = &c->fields[i];
        if (!(f->access & ACC_STATIC)) continue;
        f->slot = (uint16_t)ss;
        ss += descSlotWidth(f->desc[0]);
    }
    c->nStaticSlots = ss;
    if (ss > 0) {
        c->statics = (Slot*)vmAllocMem(sizeof(Slot) * ss);
        memset(c->statics, 0, sizeof(Slot) * ss);
        for (int i = 0; i < nF; i++) {
            Field* f = &c->fields[i];
            if (!(f->access & ACC_STATIC) || cvIdx[i] <= 0) continue;
            CpEntry* e = &cp[cvIdx[i]];
            switch (f->desc[0]) {
            case 'J': c->statics[f->slot].i = e->ival; break;
            case 'D': c->statics[f->slot].d = e->dval; break;
            case 'F': c->statics[f->slot].i = e->ival; break; /* 位模式 */
            default:  c->statics[f->slot].i = e->ival; break;
            }
        }
    }
    free(cvIdx);

    /* 方法 */
    uint16_t nM = ru16(&r);
    c->methods = (Method*)vmAllocMem(sizeof(Method) * (nM ? nM : 1));
    c->nMethods = nM;
    for (int i = 0; i < nM; i++) {
        Method* m = &c->methods[i];
        m->clazz = c;
        m->access = ru16(&r);
        char* mn = cpUtf8Str(cp, nCp, ru16(&r), c->name);
        char* md = cpUtf8Str(cp, nCp, ru16(&r), c->name);
        if (!mn || !md) return NULL;
        m->name = vmStrdup(mn); m->desc = vmStrdup(md);
        m->nargs = descArgSlots(md) + ((m->access & ACC_STATIC) ? 0 : 1);
        uint16_t nAttrs = ru16(&r);
        for (int a = 0; a < nAttrs; a++) {
            char* an = cpUtf8Str(cp, nCp, ru16(&r), c->name);
            uint32_t alen = ru32(&r);
            if (an && strcmp(an, "Code") == 0) {
                m->maxStack = ru16(&r);
                m->maxLocals = ru16(&r);
                uint32_t cl = ru32(&r);
                m->codeLen = cl;
                m->code = (uint8_t*)vmAllocMem(cl ? cl : 1);
                for (uint32_t k = 0; k < cl; k++) m->code[k] = ru8(&r);
                uint16_t nE = ru16(&r);
                m->nExc = nE;
                m->exc = (uint32_t*)vmAllocMem(sizeof(uint32_t) * 4 * (nE ? nE : 1));
                for (int k = 0; k < nE; k++) {
                    m->exc[k * 4 + 0] = ru16(&r);
                    m->exc[k * 4 + 1] = ru16(&r);
                    m->exc[k * 4 + 2] = ru16(&r);
                    m->exc[k * 4 + 3] = ru16(&r);
                }
                uint16_t nCA = ru16(&r);
                for (int k = 0; k < nCA; k++) {
                    ru16(&r);
                    uint32_t sl = ru32(&r);
                    r.p += sl; r.left -= sl;
                }
            } else {
                r.p += alen; r.left -= alen;
            }
        }
    }

    /* 类属性（忽略剩余） */

    bindNatives(c);

    /* 挂到全局类链 */
    c->next = vmG.classes;
    vmG.classes = c;

    /* 父类（Object 无父类） */
    if (superIdx) {
        if (superIdx >= nCp || cp[superIdx].tag != 7) {
            vmLog("[class] super_class 索引无效 (%d) @ %s", superIdx, c->name);
            return NULL;
        }
        char* sn = cpUtf8Str(cp, nCp, cp[superIdx].idx1, c->name);
        if (!sn) return NULL;
        c->super = vmFindClass(sn);
        if (!c->super) { vmLog("[class] 父类加载失败 %s : %s", c->name, sn); return NULL; }
    }
    return c;
}

/* ============ 类加载入口 ============ */

Class* vmFindClass(const char* name) {
    /* 兼容点分写法（manifest MIDlet-1 / 命令行参数给的是 a.b.C），统一转内部 a/b/C */
    char norm[512];
    size_t ni = 0;
    for (const char* p = name; *p && ni < sizeof(norm) - 1; p++)
        norm[ni++] = (*p == '.') ? '/' : *p;
    norm[ni] = 0;
    name = norm;
    for (Class* c = vmG.classes; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    Class* c = cfLoadClass(name);
    if (!c) return NULL;
    if (cfLinkClass(c) != 0) return NULL;
    return c;
}

Class* cfLoadClass(const char* name) {
    size_t len = 0;
    uint8_t* data = zipReadClass(name, &len);
    if (!data) {
        vmLog("[class] 找不到类 %s", name);
        return NULL;
    }
    Class* c = parseClass(data, len);
    free(data);
    return c;
}

/* ============ 链接 ============ */

int cfLinkClass(Class* c) {
    if (c->vtable) return 0;

    /* 接口：cp 索引 -> Class* */
    for (int i = 0; i < c->nIfaces; i++) {
        intptr_t idx = (intptr_t)c->ifaces[i];
        if (idx > 0 && idx < c->nCp && c->cp[idx].tag == 7) {
            char* n = cpUtf8Str(c->cp, c->nCp, c->cp[idx].idx1, c->name);
            Class* ifc = n ? vmFindClass(n) : NULL;
            c->ifaces[i] = ifc;
            if (!ifc) { vmLog("[class] 接口加载失败 %s : %s", c->name, n ? n : "?"); return -1; }
        }
    }

    /* 实例字段布局 */
    int slot = c->super ? c->super->instSlots : 0;
    for (int i = 0; i < c->nFields; i++) {
        Field* f = &c->fields[i];
        if (f->access & ACC_STATIC) continue;
        f->slot = (uint16_t)slot;
        slot += descSlotWidth(f->desc[0]);
    }
    c->instSlots = slot;

    /* vtable */
    int vs = c->super ? c->super->vtableSize : 0;
    c->vtableSize = vs;
    c->vtable = (Method**)vmAllocMem(sizeof(Method*) * (size_t)(vs + c->nMethods + 1));
    if (c->super && vs) memcpy(c->vtable, c->super->vtable, sizeof(Method*) * vs);
    for (int i = 0; i < c->nMethods; i++) {
        Method* m = &c->methods[i];
        if (m->access & ACC_STATIC) continue;
        if (m->access & ACC_PRIVATE) continue;
        if (m->name[0] == '<') continue;
        int found = -1;
        for (int k = 0; k < c->vtableSize; k++) {
            Method* om = c->vtable[k];
            if (om && strcmp(om->name, m->name) == 0 && strcmp(om->desc, m->desc) == 0) { found = k; break; }
        }
        if (found >= 0) {
            m->vtableIdx = (uint16_t)found;
            c->vtable[found] = m;
        } else {
            m->vtableIdx = (uint16_t)c->vtableSize;
            c->vtable[c->vtableSize++] = m;
        }
    }
    return 0;
}

/* ============ 初始化（<clinit>） ============ */

int cfInitClass(VMThread* t, Class* c) {
    if (!c) return -1;
    if (c->state == 1) return 0;
    if (c->state == 2) return 0;
    c->state = 2;
    if (c->super && cfInitClass(t, c->super) != 0) { c->state = 0; return -1; }
    Method* ci = vmFindMethod(c, "<clinit>", "()V");
    if (ci) {
        Slot noargs;
        vmCall(t, ci, &noargs);
        if (vmHasException(t)) {
            vmLog("[class] <clinit> 异常 %s", c->name);
            vmTakeException(t);
            c->state = 0;
            return -1;
        }
    }
    c->state = 1;
    return 0;
}

/* ============ 数组伪类 ============ */

static Class* makeArrayClass(const char* desc);

Class* vmFindArrayClass(const char* desc) {
    for (Class* c = vmG.classes; c; c = c->next)
        if (c->isArray && strcmp(c->name, desc) == 0) return c;
    return makeArrayClass(desc);
}

Class* vmArrayClassOf(Class* elem) {
    char buf[600];
    snprintf(buf, sizeof(buf), "[L%s;", elem->name);
    return vmFindArrayClass(buf);
}

static int arrayDescElemKind(const char* desc) {
    switch (desc[1]) {
        case 'B': case 'Z': return EK_BYTE;
        case 'C': return EK_CHAR;
        case 'S': return EK_SHORT;
        case 'I': return EK_INT;
        case 'J': return EK_LONG;
        case 'F': return EK_FLOAT;
        case 'D': return EK_DOUBLE;
        default:  return EK_OBJ;
    }
}

static Class* makeArrayClass(const char* desc) {
    Class* c = (Class*)vmAllocMem(sizeof(Class));
    c->name = vmStrdup(desc);
    c->isArray = 1;
    c->elemKind = (uint8_t)arrayDescElemKind(desc);
    if (desc[1] == 'L') {
        char elem[560];
        const char* s = desc + 2;
        size_t l = strlen(s);
        if (l > 0 && s[l - 1] == ';') l--;
        if (l >= sizeof(elem)) l = sizeof(elem) - 1;
        memcpy(elem, s, l); elem[l] = 0;
        c->elemClass = vmFindClass(elem);
    } else if (desc[1] == '[') {
        c->elemClass = vmFindArrayClass(desc + 1);
    }
    if (vmG.clsObject) c->super = vmG.clsObject;
    c->next = vmG.classes;
    vmG.classes = c;
    return c;
}

/* ============ 类型关系 ============ */

int vmIsSubclass(Class* sub, Class* sup) {
    if (!sub || !sup) return 0;
    for (Class* c = sub; c; c = c->super)
        if (c == sup) return 1;
    for (Class* c = sub; c; c = c->super)
        for (int i = 0; i < c->nIfaces; i++)
            if (c->ifaces[i] && vmIsSubclass(c->ifaces[i], sup)) return 1;
    return 0;
}
