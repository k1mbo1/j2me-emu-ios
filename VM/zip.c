/* zip.c - jar (zip) 类路径读取，基于 miniz (public domain) */
#include "vm_internal.h"
#include "miniz/miniz.h"

typedef struct JarEntry {
    char path[512];
    mz_zip_archive zip;
    int  valid;
    struct JarEntry* next;
} JarEntry;

/* 添加 jar 到类路径（头部插入，优先查找） */
int zipAddJar(const char* path) {
    JarEntry* j = (JarEntry*)vmAllocMem(sizeof(JarEntry));
    strncpy(j->path, path, sizeof(j->path) - 1);
    memset(&j->zip, 0, sizeof(j->zip));
    j->valid = mz_zip_reader_init_file(&j->zip, path, 0) ? 1 : 0;
    if (!j->valid) {
        vmLog("[zip] 无法打开 jar: %s", path);
        free(j);
        return -1;
    }
    j->next = vmG.jars;
    vmG.jars = j;
    return 0;
}

/* 公共 API（vm.h）*/
int vmAddJar(const char* path) { return zipAddJar(path); }

/* 关闭全部 jar（vmShutdown 用） */
void zipCloseAll(void) {
    JarEntry* j = vmG.jars;
    while (j) {
        JarEntry* n = j->next;
        if (j->valid) mz_zip_reader_end(&j->zip);
        free(j);
        j = n;
    }
    vmG.jars = NULL;
}

/* 在所有 jar 中查找条目（name 形如 "java/lang/String.class"），返回 malloc 的内容 */
uint8_t* zipReadClass(const char* classBinName, size_t* outLen) {
    char namebuf[600];
    snprintf(namebuf, sizeof(namebuf), "%s.class", classBinName);
    JarEntry* j = vmG.jars;
    while (j) {
        if (j->valid) {
            int idx = mz_zip_reader_locate_file(&j->zip, namebuf, NULL, 0);
            if (idx >= 0) {
                size_t sz = 0;
                void* p = mz_zip_reader_extract_to_heap(&j->zip, idx, &sz, 0);
                if (p) { *outLen = sz; return (uint8_t*)p; }
            }
        }
        j = j->next;
    }
    return NULL;
}

/* 读取 jar 内任意文件（manifest、资源等），jarPath 为 NULL 表示搜全部 */
uint8_t* zipReadFile(const char* jarPath, const char* entryName, size_t* outLen) {
    JarEntry* j = vmG.jars;
    while (j) {
        if (j->valid && (jarPath == NULL || strcmp(j->path, jarPath) == 0)) {
            int idx = mz_zip_reader_locate_file(&j->zip, entryName, NULL, 0);
            if (idx >= 0) {
                size_t sz = 0;
                void* p = mz_zip_reader_extract_to_heap(&j->zip, idx, &sz, 0);
                if (p) { *outLen = sz; return (uint8_t*)p; }
                if (jarPath) return NULL;
            }
        }
        j = j->next;
    }
    return NULL;
}

/* 游戏 jar 路径（列表头，即最后添加的；用于资源定位） */
const char* zipGameJarPath(void) {
    return vmG.jars ? vmG.jars->path : NULL;
}
