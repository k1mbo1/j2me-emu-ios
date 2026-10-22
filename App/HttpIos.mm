/* HttpIos.mm - NSURLSession 同步 http（隔离 ObjC，供 iOSBridgeCore.c 调用） */
#import <Foundation/Foundation.h>
#import "HttpIos.h"
#include <stdlib.h>
#include <string.h>

static char* dupNs(NSString* s) {
    if (!s) s = @"";
    NSData* d = [s dataUsingEncoding:NSUTF8StringEncoding];
    char* out = (char*)malloc([d length] + 1);
    if (!out) return NULL;
    memcpy(out, [d bytes], [d length]);
    out[[d length]] = 0;
    return out;
}

int httpIosPerform(const char* url, const char* method, const char* headers,
                   const uint8_t* body, size_t bodyLen, HttpIosResult* out) {
    memset(out, 0, sizeof(*out));
    out->status = -1;
    @autoreleasepool {
        if (!url || !method) return -1;
        NSURL* nsUrl = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
        if (!nsUrl) return -1;
        NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:nsUrl];
        req.HTTPMethod = [NSString stringWithUTF8String:method];
        req.timeoutInterval = 30.0;
        if (headers && *headers) {
            NSArray* parts = [[NSString stringWithUTF8String:headers]
                componentsSeparatedByString:@"\r\n"];
            for (NSUInteger i = 0; i + 1 < [parts count]; i += 2) {
                if ([parts[i] length] == 0) continue;
                [req setValue:parts[i + 1] forHTTPHeaderField:parts[i]];
            }
        }
        if (body && bodyLen > 0) {
            req.HTTPBody = [NSData dataWithBytes:body length:bodyLen];
        }

        __block NSData* respData = nil;
        __block NSHTTPURLResponse* httpResp = nil;
        __block NSError* err = nil;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        NSURLSessionDataTask* task = [[NSURLSession sharedSession]
            dataTaskWithRequest:req completionHandler:^(NSData* data, NSURLResponse* resp, NSError* e) {
                respData = data;
                httpResp = [resp isKindOfClass:[NSHTTPURLResponse class]]
                         ? (NSHTTPURLResponse*)resp : nil;
                err = e;
                dispatch_semaphore_signal(sem);
            }];
        [task resume];
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

        if (err || !httpResp) return -1;

        out->code = (int)httpResp.statusCode;
        out->msg = dupNs([NSHTTPURLResponse localizedStringForStatusCode:out->code]);
        NSMutableString* hdr = [NSMutableString string];
        for (NSString* k in httpResp.allHeaderFields) {
            [hdr appendFormat:@"%@: %@\r\n", k, [httpResp.allHeaderFields objectForKey:k]];
        }
        out->headers = dupNs(hdr);
        out->bodyLen = [respData length];
        out->body = (uint8_t*)malloc(out->bodyLen ? out->bodyLen : 1);
        if (out->body && out->bodyLen) memcpy(out->body, [respData bytes], out->bodyLen);
        if (!out->msg || !out->headers || !out->body) { httpIosFree(out); return -1; }
        out->status = 0;
        return 0;
    }
}

void httpIosFree(HttpIosResult* r) {
    if (!r) return;
    free(r->msg);
    free(r->headers);
    free(r->body);
    r->msg = NULL;
    r->headers = NULL;
    r->body = NULL;
    r->bodyLen = 0;
}
