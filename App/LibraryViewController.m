/* LibraryViewController.m - 游戏库：Documents/games 列表 + 导入/删除 + 启动
 * 首次启动自动安装内置示例贪吃蛇。每个游戏独立 RMS 存档目录。
 */
#import "LibraryViewController.h"
#import "GameViewController.h"

@interface LibraryViewController () <UIDocumentPickerDelegate>
@property (nonatomic, strong) NSMutableArray<NSString*>* games;   /* 文件名列表 */
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

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"Java 模拟器";
    [self.tableView registerClass:[UITableViewCell class] forCellReuseIdentifier:@"game"];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
        target:self action:@selector(importTapped:)];
    self.clearsSelectionOnViewWillAppear = YES;
    [self installSampleIfFirstRun];
    [self reload];
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
    NSMutableArray* jars = [NSMutableArray array];
    for (NSString* f in files) {
        if ([[f.lowercaseString pathExtension] isEqualToString:@"jar"]) [jars addObject:f];
    }
    [jars sortUsingSelector:@selector(localizedCompare:)];
    self.games = jars;
    [self.tableView reloadData];
}

/* ============ 启动游戏 ============ */

- (void)startGame:(NSString*)fileName {
    NSString* jarPath = [kGamesDir() stringByAppendingPathComponent:fileName];
    /* 每游戏独立 RMS 目录 */
    NSString* rmsName = [[fileName stringByDeletingPathExtension]
        stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    NSString* rmsDir = [kRmsRoot() stringByAppendingPathComponent:rmsName];
    [[NSFileManager defaultManager] createDirectoryAtPath:rmsDir
        withIntermediateDirectories:YES attributes:nil error:nil];
    GameViewController* gvc = [[GameViewController alloc]
        initWithJarPath:jarPath
        gameName:[fileName stringByDeletingPathExtension]
        rmsDir:rmsDir];
    [self.navigationController pushViewController:gvc animated:YES];
}

/* ============ 导入 ============ */

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
    UITableViewCell* cell = [tv dequeueReusableCellWithIdentifier:@"game" forIndexPath:ip];
    cell.textLabel.text = self.games[ip.row];
    cell.textLabel.font = [UIFont systemFontOfSize:17];
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
    NSString* name = self.games[ip.row];
    NSFileManager* fm = [NSFileManager defaultManager];
    [fm removeItemAtPath:[kGamesDir() stringByAppendingPathComponent:name] error:nil];
    /* 存档一并删除 */
    NSString* rmsName = [[name stringByDeletingPathExtension]
        stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    [fm removeItemAtPath:[kRmsRoot() stringByAppendingPathComponent:rmsName] error:nil];
    [self reload];
}

@end
