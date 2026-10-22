/* AppDelegate.m - 根窗口 + jar 文件打开入口 */
#import "AppDelegate.h"
#import "LibraryViewController.h"

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
 didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    /* 全局深色主题（游戏机风格，列表/弹窗自动跟随） */
    if (@available(iOS 13.0, *)) {
        self.window.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    }
    self.window.rootViewController = [[UINavigationController alloc]
        initWithRootViewController:[[LibraryViewController alloc] initWithStyle:UITableViewStylePlain]];
    [self.window makeKeyAndVisible];
    return YES;
}

/* 从"文件"App / AirDrop 打开 jar */
- (BOOL)application:(UIApplication*)app openURL:(NSURL*)url
        options:(NSDictionary<UIApplicationOpenURLOptionsKey, id>*)options {
    UINavigationController* nav = (UINavigationController*)self.window.rootViewController;
    [nav popToRootViewControllerAnimated:NO];
    if ([nav.topViewController isKindOfClass:[LibraryViewController class]]) {
        [(LibraryViewController*)nav.topViewController importJarFromURL:url];
    }
    return YES;
}

@end
