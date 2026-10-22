/* GameViewController.h - 全屏游戏页 */
#import <UIKit/UIKit.h>

@interface GameViewController : UIViewController
- (instancetype)initWithJarPath:(NSString*)jarPath gameName:(NSString*)gameName rmsDir:(NSString*)rmsDir;
@end
