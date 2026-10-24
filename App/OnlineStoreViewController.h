/* OnlineStoreViewController.h - 在线游戏仓库：分类浏览 + 搜索 + 分页列表 */
#import <UIKit/UIKit.h>

@interface OnlineStoreViewController : UITableViewController
/* categoryPath 传 nil 表示根（根分类 + 全部游戏）；钻取子分类时传完整路径 */
- (instancetype)initWithCategoryPath:(NSString*)path title:(NSString*)title;
@end
