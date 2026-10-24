/* RetroBox.m - 在线游戏仓库 API 客户端实现
 * 要点：UA 用浏览器样式；query 值含 emoji/斜杠/加号须完整编码；
 *      jarUrl 服务端已编码，直接拼接不可二次编码。
 */
#import "RetroBox.h"

static NSString* const kBase = @"https://java.iwkk.cn";
static NSString* const kUA =
    @"Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X) "
     "AppleWebKit/605.1.15 Mobile/15E148 Safari/604.1";

static NSCache* iconCache(void) {
    static NSCache* c = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        c = [[NSCache alloc] init];
        c.countLimit = 400;
    });
    return c;
}

@implementation RetroBox

+ (NSString*)baseURL { return kBase; }

+ (NSURLSession*)session {
    static NSURLSession* s = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSURLSessionConfiguration* cfg =
            [NSURLSessionConfiguration ephemeralSessionConfiguration];
        cfg.timeoutIntervalForRequest = 30;
        cfg.timeoutIntervalForResource = 300;
        cfg.HTTPAdditionalHeaders = @{@"User-Agent": kUA};
        s = [NSURLSession sessionWithConfiguration:cfg];
    });
    return s;
}

/* query 值编码：URLQueryAllowed 基础上补 %2F %2B（分类路径含斜杠） */
+ (NSString*)encodeQueryValue:(NSString*)v {
    NSString* enc = [v stringByAddingPercentEncodingWithAllowedCharacters:
        [NSCharacterSet URLQueryAllowedCharacterSet]];
    enc = [enc stringByReplacingOccurrencesOfString:@"/" withString:@"%2F"];
    enc = [enc stringByReplacingOccurrencesOfString:@"+" withString:@"%2B"];
    return enc;
}

/* 相对 URL → 绝对（服务端路径已编码；构造失败再兜底编码一次） */
+ (NSURL*)absoluteURL:(NSString*)urlString {
    if (!urlString.length) return nil;
    NSString* full = [urlString hasPrefix:@"/"] ? [kBase stringByAppendingString:urlString] : urlString;
    NSURL* u = [NSURL URLWithString:full];
    if (!u) {
        full = [full stringByAddingPercentEncodingWithAllowedCharacters:
            [NSCharacterSet URLFragmentAllowedCharacterSet]];
        u = [NSURL URLWithString:full];
    }
    return u;
}

+ (void)getJSON:(NSString*)apiPath query:(NSDictionary*)query
     completion:(void(^)(id json, NSString* error))cb {
    NSMutableString* url = [NSMutableString stringWithFormat:@"%@%@", kBase, apiPath];
    if (query.count) {
        NSMutableArray* parts = [NSMutableArray array];
        [query enumerateKeysAndObjectsUsingBlock:^(id k, id v, BOOL* stop) {
            [parts addObject:[NSString stringWithFormat:@"%@=%@",
                k, [self encodeQueryValue:[NSString stringWithFormat:@"%@", v]]]];
        }];
        [url appendFormat:@"?%@", [parts componentsJoinedByString:@"&"]];
    }
    NSURLRequest* req = [NSURLRequest requestWithURL:[self absoluteURL:url]
                                         cachePolicy:NSURLRequestUseProtocolCachePolicy
                                     timeoutInterval:30];
    [[[self session] dataTaskWithRequest:req
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
            id json = nil;
            NSString* error = nil;
            if (err) {
                error = err.localizedDescription;
            } else {
                NSInteger code = ((NSHTTPURLResponse*)resp).statusCode;
                if (code != 200) {
                    error = [NSString stringWithFormat:@"HTTP %ld", (long)code];
                } else {
                    NSError* jerr = nil;
                    json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&jerr];
                    if (!json) error = @"数据解析失败";
                }
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                if (cb) cb(json, error);
            });
        }] resume];
}

+ (void)getData:(NSString*)urlString
     completion:(void(^)(NSData* data, NSString* error))cb {
    NSURL* u = [self absoluteURL:urlString];
    if (!u) {
        if (cb) cb(nil, @"无效地址");
        return;
    }
    NSURLRequest* req = [NSURLRequest requestWithURL:u
                                         cachePolicy:NSURLRequestUseProtocolCachePolicy
                                     timeoutInterval:60];
    [[[self session] dataTaskWithRequest:req
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* err) {
            NSString* error = nil;
            if (err) error = err.localizedDescription;
            else if (((NSHTTPURLResponse*)resp).statusCode != 200)
                error = [NSString stringWithFormat:@"HTTP %ld",
                         (long)((NSHTTPURLResponse*)resp).statusCode];
            dispatch_async(dispatch_get_main_queue(), ^{
                if (cb) cb(data, error);
            });
        }] resume];
}

+ (UIImage*)cachedIcon:(NSString*)urlString {
    return [iconCache() objectForKey:urlString];
}

+ (void)loadIcon:(NSString*)urlString completion:(void(^)(UIImage* img))cb {
    UIImage* hit = [self cachedIcon:urlString];
    if (hit) {
        if (cb) cb(hit);
        return;
    }
    [self getData:urlString completion:^(NSData* data, NSString* error) {
        UIImage* img = data ? [UIImage imageWithData:data] : nil;
        if (img) [iconCache() setObject:img forKey:urlString];
        if (cb) cb(img);
    }];
}

/* 文件名清洗：去掉文件系统非法字符 */
+ (NSString*)sanitize:(NSString*)s {
    static NSCharacterSet* bad = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        bad = [NSCharacterSet characterSetWithCharactersInString:@"/\\:?%*|\"<>"];
    });
    NSArray* parts = [s componentsSeparatedByCharactersInSet:bad];
    return [[parts componentsJoinedByString:@"_"]
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

+ (NSString*)saveNameForGame:(NSDictionary*)game {
    /* API 的 fileName 是完整路径：分类/子类/游戏名/机型变体.jar
     * 落地名 = 游戏名_机型变体_gameId.jar（gameId 保证唯一、RMS 存档互不干扰） */
    NSString* fileName = game[@"fileName"] ?: @"";
    NSString* variant = [[fileName lastPathComponent] stringByDeletingPathExtension];
    NSString* name = [self sanitize:game[@"name"] ?: @"game"];
    variant = [self sanitize:variant];
    if (variant.length > 60) variant = [variant substringToIndex:60];
    if (name.length > 40) name = [name substringToIndex:40];
    NSString* base = variant.length
        ? [NSString stringWithFormat:@"%@_%@", name, variant] : name;
    return [NSString stringWithFormat:@"%@_%@.jar", base, game[@"gameId"]];
}

+ (NSString*)jarURLForGame:(NSDictionary*)game {
    return [kBase stringByAppendingString:game[@"jarUrl"] ?: @""];
}

+ (NSString*)gamesDir {
    NSString* dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
        NSUserDomainMask, YES) firstObject];
    return [dir stringByAppendingPathComponent:@"games"];
}

@end
