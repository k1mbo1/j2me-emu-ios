/* GameDetailViewController.m - 在线游戏详情页
 * 布局（UIScrollView 纵向）：
 *   [图标 | 名称 + 厂商/版本/机型]
 *   [下载按钮（带进度）]  [状态提示]
 *   [游戏截图 横向滚动]
 *   [简介]
 * 下载：NSURLSession downloadTask 委托取进度，落地 Documents/games 后校验 zip 头，
 *      成功即变"开始游戏"并自动进入游戏（与本地库同一 RMS 规则）。
 */
#import "GameDetailViewController.h"
#import "RetroBox.h"
#import "GameViewController.h"

@interface GameDetailViewController () <NSURLSessionDownloadDelegate>
@property (nonatomic, strong) NSDictionary* game;
@property (nonatomic, strong) NSDictionary* details;   /* /details 接口（可失败） */
@property (nonatomic, strong) UIScrollView* scroll;
@property (nonatomic, strong) UIImageView* iconView;
@property (nonatomic, strong) UILabel* nameL;
@property (nonatomic, strong) UILabel* metaL;
@property (nonatomic, strong) UIButton* actionBtn;     /* 下载 / 下载中 x% / 开始游戏 */
@property (nonatomic, strong) UIProgressView* progress;
@property (nonatomic, strong) UILabel* statusL;
@property (nonatomic, strong) UIScrollView* shots;     /* 截图横滚 */
@property (nonatomic, strong) UILabel* descL;
@property (nonatomic, strong) NSURLSession* dlSession;
@property (nonatomic, strong) NSURLSessionDownloadTask* dlTask;
@property (nonatomic, assign) BOOL downloaded;
@property (nonatomic, assign) BOOL downloading;
@end

@implementation GameDetailViewController

- (instancetype)initWithGame:(NSDictionary*)game {
    self = [super initWithNibName:nil bundle:nil];
    if (self) _game = game;
    return self;
}

- (NSString*)saveName { return [RetroBox saveNameForGame:self.game]; }
- (NSString*)savePath {
    return [[RetroBox gamesDir] stringByAppendingPathComponent:[self saveName]];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithWhite:0.07 alpha:1];
    self.title = self.game[@"name"];

    _scroll = [[UIScrollView alloc] init];
    _scroll.backgroundColor = [UIColor colorWithWhite:0.07 alpha:1];
    [self.view addSubview:_scroll];

    _iconView = [[UIImageView alloc] init];
    _iconView.contentMode = UIViewContentModeScaleAspectFill;
    _iconView.clipsToBounds = YES;
    _iconView.layer.cornerRadius = 16;
    _iconView.backgroundColor = [UIColor colorWithWhite:0.18 alpha:1];
    _iconView.image = [self placeholderIcon];
    [_scroll addSubview:_iconView];

    _nameL = [[UILabel alloc] init];
    _nameL.font = [UIFont boldSystemFontOfSize:20];
    _nameL.textColor = [UIColor whiteColor];
    _nameL.numberOfLines = 2;
    [_scroll addSubview:_nameL];

    _metaL = [[UILabel alloc] init];
    _metaL.font = [UIFont systemFontOfSize:13];
    _metaL.textColor = [UIColor colorWithWhite:0.58 alpha:1];
    _metaL.numberOfLines = 2;
    [_scroll addSubview:_metaL];

    _actionBtn = [UIButton buttonWithType:UIButtonTypeCustom];
    _actionBtn.titleLabel.font = [UIFont boldSystemFontOfSize:17];
    _actionBtn.layer.cornerRadius = 12;
    [_actionBtn addTarget:self action:@selector(actionTapped)
        forControlEvents:UIControlEventTouchUpInside];
    [_scroll addSubview:_actionBtn];

    _progress = [[UIProgressView alloc] initWithProgressViewStyle:
        UIProgressViewStyleDefault];
    _progress.progressTintColor = [UIColor colorWithRed:0.05 green:0.72 blue:0.72 alpha:1];
    _progress.trackTintColor = [UIColor colorWithWhite:0.2 alpha:1];
    _progress.hidden = YES;
    [_scroll addSubview:_progress];

    _statusL = [[UILabel alloc] init];
    _statusL.font = [UIFont systemFontOfSize:12];
    _statusL.textColor = [UIColor colorWithWhite:0.5 alpha:1];
    _statusL.numberOfLines = 0;
    [_scroll addSubview:_statusL];

    _shots = [[UIScrollView alloc] init];
    _shots.showsHorizontalScrollIndicator = NO;
    [_scroll addSubview:_shots];

    _descL = [[UILabel alloc] init];
    _descL.font = [UIFont systemFontOfSize:14];
    _descL.textColor = [UIColor colorWithWhite:0.82 alpha:1];
    _descL.numberOfLines = 0;
    [_scroll addSubview:_descL];

    [self fillBasic];
    [self refreshActionState];
    [self loadDetails];
    [self loadIcon];
}

/* ============ 数据 ============ */

- (UIImage*)placeholderIcon {
    CGSize s = CGSizeMake(112, 112);
    UIGraphicsImageRenderer* r = [[UIGraphicsImageRenderer alloc] initWithSize:s];
    return [r imageWithActions:^(UIGraphicsImageRendererContext* ctx) {
        CGRect b = CGRectMake(0, 0, s.width, s.height);
        UIBezierPath* clip = [UIBezierPath bezierPathWithRoundedRect:b cornerRadius:28];
        [clip addClip];
        NSArray* colors = @[(id)[UIColor colorWithRed:0.14 green:0.30 blue:0.85 alpha:1].CGColor,
                             (id)[UIColor colorWithRed:0.05 green:0.72 blue:0.72 alpha:1].CGColor];
        CGGradientRef grad = CGGradientCreateWithColors(nil, (__bridge CFArrayRef)colors, NULL);
        CGContextDrawLinearGradient(ctx.CGContext, grad, CGPointMake(0, 0),
            CGPointMake(0, s.height), 0);
        CGGradientRelease(grad);
        NSMutableParagraphStyle* ps = [[NSMutableParagraphStyle alloc] init];
        ps.alignment = NSTextAlignmentCenter;
        NSDictionary* at = @{NSFontAttributeName: [UIFont boldSystemFontOfSize:40],
                             NSForegroundColorAttributeName: [UIColor whiteColor],
                             NSParagraphStyleAttributeName: ps};
        [@"J2" drawInRect:CGRectMake(0, 30, s.width, 52) withAttributes:at];
    }];
}

- (void)fillBasic {
    _nameL.text = self.game[@"name"];
    NSString* variant = [[self.game[@"fileName"] ?: @"" lastPathComponent]
        stringByDeletingPathExtension];
    _metaL.text = [NSString stringWithFormat:@"%@ · v%@\n机型：%@",
        self.game[@"vendor"] ?: @"未知厂商",
        self.game[@"version"] ?: @"1.0",
        variant.length ? variant : @"通用版"];
}

- (void)loadIcon {
    NSString* url = self.game[@"iconUrl"];
    if (!url.length) return;
    __weak GameDetailViewController* ws = self;
    [RetroBox loadIcon:url completion:^(UIImage* img) {
        if (ws && img) ws.iconView.image = img;
    }];
}

- (void)loadDetails {
    __weak GameDetailViewController* ws = self;
    NSString* path = [NSString stringWithFormat:@"/api/games/%@/details",
        self.game[@"gameId"]];
    [RetroBox getJSON:path query:nil completion:^(id json, NSString* error) {
        GameDetailViewController* s = ws;
        if (!s || ![json isKindOfClass:[NSDictionary class]]) return;
        s.details = json;
        /* 简介 */
        NSString* desc = json[@"description"];
        s.descL.text = (desc.length
            && ![desc isEqualToString:@"无"]) ? desc : @"暂无简介";
        /* 截图 */
        NSArray* shots = json[@"screenshots"];
        if ([shots isKindOfClass:[NSArray class]] && shots.count) [s buildShots:shots];
        [s layoutContent];
    }];
}

- (void)buildShots:(NSArray*)urls {
    CGFloat x = 0;
    for (NSString* u in urls) {
        if (![u isKindOfClass:[NSString class]]) continue;
        UIImageView* iv = [[UIImageView alloc] initWithFrame:CGRectMake(x, 0, 150, 200)];
        iv.contentMode = UIViewContentModeScaleAspectFit;
        iv.backgroundColor = [UIColor colorWithWhite:0.12 alpha:1];
        iv.layer.cornerRadius = 8;
        iv.clipsToBounds = YES;
        [_shots addSubview:iv];
        x += 158;
        [RetroBox getData:u completion:^(NSData* data, NSString* err) {
            if (data) {
                UIImage* img = [UIImage imageWithData:data];
                if (img) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        iv.image = img;
                    });
                }
            }
        }];
    }
    _shots.contentSize = CGSizeMake(MAX(x, 1), 200);
}

/* ============ 布局 ============ */

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [self layoutContent];
}

- (void)layoutContent {
    CGRect vb = self.view.bounds;
    CGFloat top = CGRectGetMaxY(self.navigationController.navigationBar.frame);
    if (top <= 0) top = 64;
    CGFloat w = vb.size.width;
    _scroll.frame = CGRectMake(0, top, w, vb.size.height - top);

    CGFloat y = 16;
    _iconView.frame = CGRectMake(16, y, 72, 72);
    _nameL.frame = CGRectMake(104, y + 2, w - 120, 26);
    _metaL.frame = CGRectMake(104, y + 32, w - 120, 36);
    y += 72 + 16;

    _actionBtn.frame = CGRectMake(16, y, w - 32, 46);
    y += 46 + 8;
    _progress.frame = CGRectMake(16, y, w - 32, 4);
    y += 4 + 6;
    _statusL.frame = CGRectMake(16, y, w - 32, 16);
    y += 16 + 16;

    if (_shots.subviews.count) {
        _shots.hidden = NO;
        _shots.frame = CGRectMake(0, y, w, 200);
        y += 200 + 8;
    } else {
        _shots.hidden = YES;
    }

    [_descL sizeToFit];
    CGRect df = _descL.frame;
    df.origin = CGPointMake(16, y);
    df.size.width = w - 32;
    [_descL setFrame:df];
    [_descL sizeToFit];
    y = CGRectGetMaxY(_descL.frame) + 32;

    _scroll.contentSize = CGSizeMake(w, MAX(y, vb.size.height - top + 1));
}

/* ============ 下载 / 开始游戏 ============ */

- (void)refreshActionState {
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:[self savePath]];
    self.downloaded = exists;
    if (self.downloading) return;
    if (exists) {
        [_actionBtn setTitle:@"开始游戏" forState:UIControlStateNormal];
        [_actionBtn setBackgroundColor:[UIColor colorWithRed:0.05 green:0.72 blue:0.72 alpha:1]];
        _progress.hidden = YES;
        _statusL.text = @"已下载到本地游戏库";
    } else {
        [_actionBtn setTitle:@"下载并开始" forState:UIControlStateNormal];
        [_actionBtn setBackgroundColor:[UIColor colorWithRed:0.14 green:0.30 blue:0.85 alpha:1]];
        _progress.hidden = YES;
        _statusL.text = @"下载后保存到本地游戏库，随时可玩";
    }
}

- (void)actionTapped {
    if (self.downloading) return;
    if (self.downloaded) {
        [self launchGame];
        return;
    }
    [self startDownload];
}

- (void)cleanupSession {
    if (_dlSession) [_dlSession finishTasksAndInvalidate];
    _dlSession = nil;
    _dlTask = nil;
}

- (void)startDownload {
    NSURL* url = [NSURL URLWithString:[RetroBox jarURLForGame:self.game]];
    if (!url) {
        [self showError:@"下载地址无效"];
        return;
    }
    NSURLSessionConfiguration* cfg =
        [NSURLSessionConfiguration defaultSessionConfiguration];
    cfg.timeoutIntervalForResource = 600;
    _dlSession = [NSURLSession sessionWithConfiguration:cfg delegate:self delegateQueue:nil];
    _dlTask = [_dlSession downloadTaskWithURL:url];
    [_dlTask resume];
    self.downloading = YES;
    _actionBtn.enabled = NO;
    [_actionBtn setTitle:@"准备下载…" forState:UIControlStateNormal];
    [_actionBtn setBackgroundColor:[UIColor colorWithWhite:0.25 alpha:1]];
    _progress.hidden = NO;
    _progress.progress = 0;
    _statusL.text = @"正在下载…";
}

- (void)URLSession:(NSURLSession*)session
      downloadTask:(NSURLSessionDownloadTask*)task
 didWriteData:(int64_t)written
 totalBytesWritten:(int64_t)totalWritten
 totalBytesExpectedToWrite:(int64_t)totalExpected {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (totalExpected > 0) {
            double p = (double)totalWritten / (double)totalExpected;
            _progress.progress = (float)p;
            NSString* size = totalWritten >= 1048576
                ? [NSString stringWithFormat:@"%.1f MB", totalWritten / 1048576.0]
                : [NSString stringWithFormat:@"%ld KB", (long)(totalWritten / 1024)];
            [_actionBtn setTitle:[NSString stringWithFormat:@"下载中 %ld%% · %@",
                (long)(p * 100), size] forState:UIControlStateNormal];
        }
    });
}

- (void)URLSession:(NSURLSession*)session
      downloadTask:(NSURLSessionDownloadTask*)task
didFinishDownloadingToURL:(NSURL*)location {
    /* 临时文件在回调返回后即被系统回收，必须先同步读入内存 */
    NSData* data = [NSData dataWithContentsOfURL:location];
    dispatch_async(dispatch_get_main_queue(), ^{
        self.downloading = NO;
        _actionBtn.enabled = YES;
        if (!data || data.length < 4) {
            [self cleanupSession];
            [self showError:@"下载内容为空"];
            [self refreshActionState];
            return;
        }
        /* 校验 zip 魔数（jar 即 zip） */
        const uint8_t* p = (const uint8_t*)data.bytes;
        if (!(p[0] == 'P' && p[1] == 'K')) {
            [self cleanupSession];
            [self showError:@"文件校验失败（非有效 jar）"];
            [self refreshActionState];
            return;
        }
        [[NSFileManager defaultManager] createDirectoryAtPath:[RetroBox gamesDir]
            withIntermediateDirectories:YES attributes:nil error:nil];
        NSString* dst = [self savePath];
        if ([[NSFileManager defaultManager] fileExistsAtPath:dst]) {
            [[NSFileManager defaultManager] removeItemAtPath:dst error:nil];
        }
        BOOL ok = [data writeToFile:dst atomically:YES];
        if (!ok) {
            [self cleanupSession];
            [self showError:@"保存文件失败"];
            [self refreshActionState];
            return;
        }
        [self cleanupSession];
        [self refreshActionState];
        _statusL.text = @"下载完成，正在启动…";
        [self launchGame];   /* 一键下载直接玩 */
    });
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task
didCompleteWithError:(NSError*)error {
    if (!error) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        self.downloading = NO;
        _actionBtn.enabled = YES;
        [self cleanupSession];
        [self refreshActionState];
        [self showError:[NSString stringWithFormat:@"下载失败：%@",
            error.localizedDescription]];
    });
}

- (void)showError:(NSString*)msg {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:nil
        message:msg preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"好" style:UIAlertActionStyleDefault
        handler:nil]];
    [self presentViewController:a animated:YES completion:nil];
}

/* 与本地游戏库同规则：RMS 存档目录 = Documents/rms/<落地文件名去扩展名> */
- (void)launchGame {
    NSString* jarPath = [self savePath];
    NSString* rmsRoot = [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
        NSUserDomainMask, YES) firstObject] stringByAppendingPathComponent:@"rms"];
    NSString* rmsName = [[[self saveName] stringByDeletingPathExtension]
        stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
    NSString* rmsDir = [rmsRoot stringByAppendingPathComponent:rmsName];
    [[NSFileManager defaultManager] createDirectoryAtPath:rmsDir
        withIntermediateDirectories:YES attributes:nil error:nil];
    GameViewController* gvc = [[GameViewController alloc]
        initWithJarPath:jarPath
        gameName:self.game[@"name"]
        rmsDir:rmsDir];
    [self.navigationController pushViewController:gvc animated:YES];
}

@end
