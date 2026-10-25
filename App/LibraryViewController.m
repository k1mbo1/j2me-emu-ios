/* LibraryViewController.m - 游戏库：Documents/games 列表 + 导入/删除 + 启动
 * v1.0.3：列表显示 MIDlet 显示名（直读 jar manifest）+ 文件名/大小副标题 +
 *        抽象方块图标 + 按显示名排序 + 空状态提示。
 * v1.0.4：新增"在线仓库"入口（左上角），浏览/搜索/下载 RetroBox 全库游戏。
 * 首次启动自动安装内置示例贪吃蛇。每个游戏独立 RMS 存档目录。
 */
#import "LibraryViewController.h"
#import "GameViewController.h"
#import "OnlineStoreViewController.h"
#import "iOSBridge.h"

@interface LibraryViewController () <UIDocumentPickerDelegate>
/* 元素: @{@"file":文件名, @"size":字节数(NSNumber), @"name":显示名} */
@property (nonatomic, strong) NSMutableArray<NSDictionary*>* games;
@end

static NSString* kGamesDir(void) {
    NSString* dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)
        firstObject];
    return [dir stringByAppendingPathComponent:@"games"];
}
static NSString* kRmsRoot(void) {
    NSString* dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)
        firstObject];
    return [dir stringByAppendingPathComponent:@"rms"];
}

@implementation LibraryViewController

/* 抽象方块图标（圆角渐变 + "J2" 字样，避免突兀图案） */
+ (UIImage*)jarIcon {
    static UIImage* icon = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        CGSize s = CGSizeMake(58, 58);
        UIGraphicsImageRenderer* r = [[UIGraphicsImageRenderer alloc] initWithSize:s];
        icon = [r imageWithActions:^(UIGraphicsImageRendererContext* ctx) {
            CGRect b = CGRectMake(0, 0, s.width, s.height);
            UIBezierPath* clip = [UIBezierPath bezierPathWithRoundedRect:b cornerRadius:14];
            [clip addClip];
            NSArray* colors = @[(id)[UIColor colorWithRed:0.14 green:0.30 blue:0.85 alpha:1].CGColor,
                                 (id)[UIColor colorWithRed:0.05 green:0.72 blue:0.72 alpha:1].CGColor];
            CGGradientRef grad = CGGradientCreateWithColors(nil, (__bridge CFArrayRef)colors, NULL);
            CGContextDrawLinearGradient(ctx.CGContext, grad, CGPointMake(0, 0),
                CGPointMake(0, s.height), 0);
            CGGradientRelease(grad);
            NSMutableParagraphStyle* ps = [[NSMutableParagraphStyle alloc] init];
            ps.alignment = NSTextAlignmentCenter;
            NSDictionary* at = @{NSFontAttributeName: [UIFont boldSystemFontOfSize:20],
                                 NSForegroundColorAttributeName: [UIColor whiteColor],
                                 NSParagraphStyleAttributeName: ps};
            [@"J2" drawInRect:CGRectMake(0, 15, s.width, 28) withAttributes:at];
        }];
    });
    return icon;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:@"在线仓库" style:UIBarButtonItemStylePlain
        target:self action:@selector(storeTapped:)];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
        target:self action:@selector(importTapped:)];
    self.clearsSelectionOnViewWillAppear = YES;
    self.tableView.rowHeight = 62;
    self.tableView.separatorInset = UIEdgeInsetsMake(0, 76, 0, 0);
    [self installSampleIfFirstRun];
    [self reload];

    /* e2e 自动化：--e2e-autostart <文件名> 延迟启动指定游戏（CI 模拟器验证用） */
    NSArray* args = [[NSProcessInfo processInfo] arguments];
    NSUInteger ai = [args indexOfObject:@"--e2e-autostart"];
    if (ai != NSNotFound && ai + 1 < args.count) {
        NSString* want = args[ai + 1];
        __weak typeof(self) ws = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.2 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            typeof(self) s = ws;
            if (!s) return;
            for (NSDictionary* g in s.games) {
                if ([g[@"file"] isEqualToString:want]) {
                    [s startGame:g];
                    return;
                }
            }
        });
    }

    /* --e2e-keys 的按键注入已移到 GameViewController（以游戏启动为计时基准，
     * 游戏库/VM 冷启动慢时不会把按键提前到 VM 就绪之前） */
}

/* 首次启动：拷贝内置示例贪吃蛇到游戏库 */
- (void)installSampleIfFirstRun {
    NSUserDefaults* defs = [NSUserDefaults standardUserDefaults];
    if ([defs boolForKey:@"sampleInstalled"]) return;
    [defs setBool:YES forKey:@"sampleInstalled"];
    NSString* src = [[NSBundle mainBundle] pathForResource:@"snake" ofType:@"jar"];
    if (!src) return;
    NSString* dst = [kGamesDir() stringByAppendingPathComponent:@"示例贪吃蛇.jar"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:dst]) {
        [self copyJar:src toName:@"示例贪吃蛇.jar"];
    }
}

- (void)reload {
    NSFileManager* fm = [NSFileManager defaultManager];
    [fm createDirectoryAtPath:kGamesDir() withIntermediateDirectories:YES attributes:nil error:nil];
    NSArray* files = [fm contentsOfDirectoryAtPath:kGamesDir() error:nil];
    NSMutableArray* arr = [NSMutableArray array];
    for (NSString* f in files) {
        if (![[f.lowercaseString pathExtension] isEqualToString:@"jar"]) continue;
        NSString* path = [kGamesDir() stringByAppendingPathComponent:f];
        unsigned long long size = 0;
        NSDictionary* attr = [fm attributesOfItemAtPath:path error:nil];
        if (attr) size = [attr fileSize];
        /* 直读 manifest 的 MIDlet-1 显示名（失败回退文件名） */
        NSString* disp = nil;
        char name[128];
        if (j2mePeekManifest(path.UTF8String, name, sizeof(name)) == 0 && name[0]) {
            disp = [NSString stringWithUTF8String:name];
        }
        [arr addObject:@{@"file": f,
                         @"size": @(size),
                         @"name": disp ?: [f stringByDeletingPathExtension]}];
    }
    /* 按显示名排序（本地化比较） */
    [arr sortUsingComparator:^NSComparisonResult(NSDictionary* a, NSDictionary* b) {
        return [a[@"name"] localizedCompare:b[@"name"]];
    }];
    self.games = arr;

    self.title = self.games.count
        ? [NSString stringWithFormat:@"游戏库 · %tu 个", self.games.count]
        : @"游戏库";

    /* 空状态提示 */
    if (self.games.count == 0) {
        UILabel* empty = [[UILabel alloc] initWithFrame:CGRectMake(0, 120,
            self.tableView.bounds.size.width, 220)];
        empty.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        empty.numberOfLines = 0;
        empty.textAlignment = NSTextAlignmentCenter;
        empty.textColor = [UIColor colorWithWhite:0.55 alpha:1];
        empty.font = [UIFont systemFontOfSize:15];
        empty.text = @"游戏库是空的\n\n点左上角『在线仓库』浏览 1.4 万+ 经典游戏\n下载后自动进入游戏库\n\n也可以点右上角 ＋ 导入 .jar 文件\n或通过\"文件\"App / AirDrop 发送到本应用";
        self.tableView.backgroundView = empty;
    } else {
        self.tableView.backgroundView = nil;
    }
    [self.tableView reloadData];
}

/* ============ 启动游戏 ============ */

- (void)startGame:(NSDictionary*)game {
    NSString* jarPath = [kGamesDir() stringByAppendingPathComponent:game[@"file"]];
    /* 每游戏独立 RMS 目录 */
    NSString* rmsName = [game[@"file"] stringByDeletingPathExtension];
    rmsName = [rmsName stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    NSString* rmsDir = [kRmsRoot() stringByAppendingPathComponent:rmsName];
    [[NSFileManager defaultManager] createDirectoryAtPath:rmsDir
        withIntermediateDirectories:YES attributes:nil error:nil];
    GameViewController* gvc = [[GameViewController alloc]
        initWithJarPath:jarPath
        gameName:game[@"name"]
        rmsDir:rmsDir];
    [self.navigationController pushViewController:gvc animated:YES];
}

/* ============ 导入 ============ */

- (void)storeTapped:(id)sender {
    OnlineStoreViewController* store = [[OnlineStoreViewController alloc]
        initWithCategoryPath:nil title:@"在线游戏仓库"];
    [self.navigationController pushViewController:store animated:YES];
}

- (void)importTapped:(id)sender {
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc]
        initWithDocumentTypes:@[@"public.data", @"com.sun.jar-archive"]
        inMode:UIDocumentPickerModeImport];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller
didPickDocumentAtURL:(NSURL*)url {
    [self importJarFromURL:url];
}

- (void)importJarFromURL:(NSURL*)url {
    if (!url) return;
    /* 取文件名（"文件"App 里的 jar 常无扩展名显示但 URL 有） */
    NSString* name = url.lastPathComponent;
    if (![[name.lowercaseString pathExtension] isEqualToString:@"jar"]) {
        name = [name stringByAppendingPathExtension:@"jar"];
    }
    if ([self copyJar:[url path] toName:name]) {
        [self reload];
    } else {
        UIAlertController* a = [UIAlertController alertControllerWithTitle:@"导入失败"
            message:@"无法复制该文件" preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"好" style:UIAlertActionStyleDefault handler:nil]];
        [self presentViewController:a animated:YES completion:nil];
    }
}

- (BOOL)copyJar:(NSString*)srcPath toName:(NSString*)name {
    NSFileManager* fm = [NSFileManager defaultManager];
    NSString* dst = [kGamesDir() stringByAppendingPathComponent:name];
    if ([dst isEqualToString:srcPath]) return YES;
    if ([fm fileExistsAtPath:dst]) [fm removeItemAtPath:dst error:nil];
    return [fm copyItemAtPath:srcPath toPath:dst error:nil];
}

/* ============ UITableView ============ */

- (NSInteger)tableView:(UITableView*)tv numberOfRowsInSection:(NSInteger)section {
    return self.games.count;
}

- (UITableViewCell*)tableView:(UITableView*)tv cellForRowAtIndexPath:(NSIndexPath*)ip {
    /* Subtitle 样式（registerClass 只出 Default 样式，手动创建） */
    UITableViewCell* cell = [tv dequeueReusableCellWithIdentifier:@"game"];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                             reuseIdentifier:@"game"];
    NSDictionary* g = self.games[ip.row];
    cell.textLabel.text = g[@"name"];
    cell.textLabel.font = [UIFont boldSystemFontOfSize:17];
    unsigned long long size = [g[@"size"] unsignedLongLongValue];
    NSString* sizeText = size >= 1048576
        ? [NSString stringWithFormat:@"%.1f MB", size / 1048576.0]
        : [NSString stringWithFormat:@"%llu KB", (size + 1023) / 1024];
    cell.detailTextLabel.text = [NSString stringWithFormat:@"%@ · %@", g[@"file"], sizeText];
    cell.detailTextLabel.font = [UIFont systemFontOfSize:13];
    cell.detailTextLabel.textColor = [UIColor colorWithWhite:0.55 alpha:1];
    cell.imageView.image = [self.class jarIcon];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
}

- (void)tableView:(UITableView*)tv didSelectRowAtIndexPath:(NSIndexPath*)ip {
    [self startGame:self.games[ip.row]];
    [tv deselectRowAtIndexPath:ip animated:YES];
}

- (void)tableView:(UITableView*)tv commitEditingStyle:(UITableViewCellEditingStyle)style
forRowAtIndexPath:(NSIndexPath*)ip {
    if (style != UITableViewCellEditingStyleDelete) return;
    NSDictionary* game = self.games[ip.row];
    NSFileManager* fm = [NSFileManager defaultManager];
    [fm removeItemAtPath:[kGamesDir() stringByAppendingPathComponent:game[@"file"]] error:nil];
    /* 存档一并删除 */
    NSString* rmsName = [game[@"file"] stringByDeletingPathExtension];
    rmsName = [rmsName stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    [fm removeItemAtPath:[kRmsRoot() stringByAppendingPathComponent:rmsName] error:nil];
    [self reload];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    /* 从游戏页返回时刷新（期间可能经"文件"App 导入过） */
    [self reload];
}

@end
