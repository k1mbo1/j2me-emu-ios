/* LibraryViewController.h - 游戏库 */
#import <UIKit/UIKit.h>

@interface LibraryViewController : UITableViewController
/* 从"文件"App 打开 jar 时由 AppDelegate 调用（导入后刷新） */
- (void)importJarFromURL:(NSURL*)url;
@end
