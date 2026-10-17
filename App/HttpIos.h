/* HttpIos.h - iOS http 执行（ObjC 隔离层，纯 C 接口，不依赖 VM 类型） */
#ifndef HTTP_IOS_H
#define HTTP_IOS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int status;                        /* 0 成功 -1 失败 */
    int code;                          /* HTTP 状态码 */
    char* msg;                         /* malloc（UTF-8），调用方 free */
    char* headers;                     /* malloc，"Key: Value\r\n" 格式，调用方 free */
    uint8_t* body;                     /* malloc，调用方 free */
    size_t bodyLen;
} HttpIosResult;

/* headers 参数格式: "k\r\nv\r\nk\r\nv\r\n"。成功返回 0。 */
int httpIosPerform(const char* url, const char* method, const char* headers,
                   const uint8_t* body, size_t bodyLen, HttpIosResult* out);

/* 释放 result 内部字段 */
void httpIosFree(HttpIosResult* r);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_IOS_H */
