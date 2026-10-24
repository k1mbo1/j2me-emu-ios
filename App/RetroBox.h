/* RetroBox.h - 在线游戏仓库（java.iwkk.cn）API 客户端 + 图标缓存
 * v1.0.4 新增。所有回调统一主线程。
 */
#import <UIKit/UIKit.h>

@interface RetroBox : NSObject

+ (NSString*)baseURL;                            /* https://java.iwkk.cn */

/* GET JSON：apiPath 形如 "/api/games"；query 值自动 urlencode */
+ (void)getJSON:(NSString*)apiPath
          query:(NSDictionary*)query
     completion:(void(^)(id json, NSString* error))cb;

/* GET 二进制：urlString 可为相对路径（以 / 开头自动拼基地址） */
+ (void)getData:(NSString*)urlString
     completion:(void(^)(NSData* data, NSString* error))cb;

/* 图标缓存（内存 NSCache）：未命中返回 nil；loadIcon 完成后主线程回调 */
+ (UIImage*)cachedIcon:(NSString*)urlString;
+ (void)loadIcon:(NSString*)urlString completion:(void(^)(UIImage* img))cb;

/* 由 API 的 game 字典生成唯一落地文件名：游戏名_机型变体_gameId.jar */
+ (NSString*)saveNameForGame:(NSDictionary*)game;
/* 完整 jar 下载 URL（jarUrl 已是编码路径，直接拼基地址） */
+ (NSString*)jarURLForGame:(NSDictionary*)game;
/* Documents/games 目录（与本地游戏库一致） */
+ (NSString*)gamesDir;

@end
