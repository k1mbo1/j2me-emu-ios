/* GameViewController.m - 全屏游戏页：DisplayLink 帧驱动 + 分离式虚拟键盘 + 蓝牙手柄 + 暂停菜单
 * v1.0.3 新增：缩放档位（适应/1x/2x/3x，双击画面切换）、OSD 提示、
 *             暂停菜单扩展（键盘/缩放/音量/截图/重启/回库）、音量弹窗、截图保存相册、横屏支持。
 * 布局（分离式，键盘不遮挡画面）:
 *   [导航栏: 返回 | 游戏名 | 键盘切换/暂停]
 *   [        GameView（最近邻缩放，档位可切）        ]
 *   [  左:方向十字+FIRE   右:数字块 1 3 / 7 9 / * 0   ]
 */
#import "GameViewController.h"
#import "iOSBridge.h"
#import <GameController/GameController.h>
#import <string.h>

/* ============ 安全区工具 ============ */
static CGFloat safeTop(UIViewController* vc) {
    if (@available(iOS 11.0, *)) {
        return vc.view.safeAreaInsets.top > 0 ? vc.view.safeAreaInsets.top : 44.0;
    }
    return 20.0;
}
static CGFloat safeBottomY(UIViewController* vc) {
    if (@available(iOS 11.0, *)) {
        return vc.view.bounds.size.height
             - (vc.view.safeAreaInsets.bottom > 0 ? vc.view.safeAreaInsets.bottom : 0.0);
    }
    return vc.view.bounds.size.height;
}

/* ============ Canvas 键码（'0'-'9'、'*'、'#'） ============ */
#define K_UP     '2'
#define K_DOWN   '8'
#define K_LEFT   '4'
#define K_RIGHT  '6'
#define K_FIRE   '5'
#define K_A      '1'
#define K_B      '3'
#define K_C      '7'
#define K_D      '9'
#define K_STAR   '*'
#define K_ZERO   '0'
#define K_POUND  '#'
/* Nokia 软键（FullCanvas 常量）：S40 国产游戏用它们开菜单/取消 */
#define K_SOFT1  (-6)
#define K_SOFT2  (-7)

#pragma mark - GameView：像素渲染（最近邻缩放）

@interface GameView : UIView
@property (nonatomic, assign) CGSize gameSize;   /* 逻辑分辨率（首帧前 240x320） */
- (void)updatePixels:(const uint32_t*)px w:(int)w h:(int)h;
@end

static void releasePixels(void* info, const void* data, size_t size) {
    (void)info; (void)size;
    free((void*)data);
}

@implementation GameView

- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _gameSize = CGSizeMake(240, 320);
        self.layer.contentsGravity = kCAGravityResize;
        self.layer.magnificationFilter = kCAFilterNearest;
        self.layer.minificationFilter = kCAFilterNearest;
        self.backgroundColor = [UIColor blackColor];
    }
    return self;
}

/* 每帧深拷贝像素 -> CGImage -> layer.contents（render server 异步消费，必须拷贝） */
- (void)updatePixels:(const uint32_t*)px w:(int)w h:(int)h {
    if (w <= 0 || h <= 0) return;
    if (self.gameSize.width != w || self.gameSize.height != h) {
        self.gameSize = CGSizeMake(w, h);
        [self.superview setNeedsLayout];
    }
    size_t sz = (size_t)w * (size_t)h * 4;
    void* copy = malloc(sz);
    if (!copy) return;
    memcpy(copy, px, sz);
    CGDataProviderRef prov = CGDataProviderCreateWithData(NULL, copy, sz, releasePixels);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef img = CGImageCreate(w, h, 8, 32, (size_t)w * 4, cs,
                                   kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst,
                                   prov, NULL, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(prov);
    if (img) {
        self.layer.contents = (__bridge id)img;
        CGImageRelease(img);
    } else {
        free(copy);
    }
}

@end

#pragma mark - KeyPadButton

@interface KeyPadButton : UIButton
@property (nonatomic, assign) int keyCode;
@end

@implementation KeyPadButton
@end

#pragma mark - GameViewController

@interface GameViewController ()
@property (nonatomic, strong) NSString* jarPath;
@property (nonatomic, strong) NSString* gameName;
@property (nonatomic, strong) NSString* rmsDir;
@property (nonatomic, strong) GameView* gameView;
@property (nonatomic, strong) UIView* keyPad;
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, assign) BOOL padVisible;
@property (nonatomic, assign) BOOL paused;
@property (nonatomic, assign) BOOL pausedByBackground;  /* 切后台导致暂停：回前台自动恢复 */
@property (nonatomic, assign) int  scaleMode;      /* 0=适应 1/2/3=倍率档 */
@property (nonatomic, assign) CGFloat lastScale;  /* 当前实际生效倍率（OSD 显示用） */
@property (nonatomic, strong) UILabel* osdLabel;
@property (nonatomic, assign) double osdUntil;
@property (nonatomic, assign) BOOL  osdHiding;
@end

/* 按住重复状态（keycode -> 时间戳，0=未按） */
static double gKeyHeld[256];
static double gKeyLastRep[256];

@implementation GameViewController

- (instancetype)initWithJarPath:(NSString*)jarPath gameName:(NSString*)gameName rmsDir:(NSString*)rmsDir {
    self = [super initWithNibName:nil bundle:nil];
    if (self) {
        _jarPath = jarPath;
        _gameName = gameName;
        _rmsDir = rmsDir;
        _padVisible = YES;
        _scaleMode = 0;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.07 green:0.07 blue:0.10 alpha:1];
    self.navigationItem.title = self.gameName;

    /* 顶栏按钮：键盘切换 + 暂停 */
    UIBarButtonItem* padBtn = [[UIBarButtonItem alloc]
        initWithTitle:@"键盘" style:UIBarButtonItemStylePlain
        target:self action:@selector(togglePad:)];
    UIBarButtonItem* pauseBtn = [[UIBarButtonItem alloc]
        initWithTitle:@"暂停" style:UIBarButtonItemStylePlain
        target:self action:@selector(showPauseMenu:)];
    self.navigationItem.rightBarButtonItems = @[pauseBtn, padBtn];

    /* 游戏画面 */
    _gameView = [[GameView alloc] initWithFrame:CGRectZero];
    _gameView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_gameView];

    /* 双击画面 = 切换缩放档位 */
    UITapGestureRecognizer* dbl = [[UITapGestureRecognizer alloc]
        initWithTarget:self action:@selector(cycleScale:)];
    dbl.numberOfTapsRequired = 2;
    [_gameView addGestureRecognizer:dbl];

    /* 虚拟键盘（分离式底部区） */
    [self buildKeyPad];
    _keyPad.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_keyPad];
    _keyPad.hidden = NO;

    /* OSD 提示条（懒创建，layoutAll 定位） */
    _osdLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _osdLabel.font = [UIFont boldSystemFontOfSize:14];
    _osdLabel.textColor = [UIColor whiteColor];
    _osdLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.65];
    _osdLabel.textAlignment = NSTextAlignmentCenter;
    _osdLabel.layer.cornerRadius = 12;
    _osdLabel.clipsToBounds = YES;
    _osdLabel.hidden = YES;
    _osdLabel.alpha = 0;
    [self.view addSubview:_osdLabel];

    [self layoutAll];

    /* 启动 MIDlet */
    NSString* midpPath = [[NSBundle mainBundle] pathForResource:@"midp" ofType:@"jar"];
    int rc = j2meStart(midpPath.UTF8String, self.jarPath.UTF8String,
                       self.rmsDir.UTF8String, 240, 320);
    if (rc != 0) {
        [self fatalAlert:[NSString stringWithUTF8String:j2meLastError()]];
        return;
    }

    /* 顶栏换成 manifest 里的 MIDlet 显示名 */
    const char* mn = j2meMidletName();
    if (mn && mn[0]) self.navigationItem.title = [NSString stringWithUTF8String:mn];

    /* 帧驱动 */
    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    /* 蓝牙手柄 */
    [self watchControllers];

    /* e2e：虚拟键盘命中与事件链路自检（CI 用，结果写 Documents/uicheck.txt） */
    [self startE2EUICheckIfNeeded];

    /* 前后台切换 */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onEnterBackground)
        name:UIApplicationDidEnterBackgroundNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onEnterForeground)
        name:UIApplicationWillEnterForegroundNotification object:nil];

    /* 启动操作提示 */
    [self showOsd:@"双击画面切换缩放 · 暂停键打开菜单"];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

/* ============ 布局 ============ */

- (void)layoutAll {
    CGRect vb = self.view.bounds;
    CGFloat top = CGRectGetMaxY(self.navigationController.navigationBar.frame);
    if (top <= 0) top = safeTop(self);
    CGFloat bottom = safeBottomY(self);
    CGFloat padH = 240.0;

    CGRect gameRect = CGRectMake(0, top, vb.size.width, bottom - top - (self.padVisible ? padH : 0));
    /* 逻辑分辨率缩放：档位 0=aspect-fit 适应；1/2/3=整数倍（超屏时降为适应，避免裁剪）。
     * 物理像素整数化：把缩放向下取整到「物理像素整数倍」，最近邻采样下每个逻辑
     * 像素占相同数量的物理像素，笔画粗细均匀、文字锐利（非整数倍会导致笔画
     * 粗细不均，观感模糊）。 */
    CGSize gs = _gameView.gameSize;
    if (gs.width < 1 || gs.height < 1) gs = CGSizeMake(240, 320);
    CGFloat fit = MIN(gameRect.size.width / gs.width, gameRect.size.height / gs.height);
    CGFloat scale = fit;
    if (self.scaleMode > 0) scale = MIN((CGFloat)self.scaleMode, fit);
    CGFloat scr = [UIScreen mainScreen].scale;   /* 物理像素/点 */
    if (scr > 0 && scale * scr >= 2)             /* 至少 2 物理像素/逻辑像素才取整 */
        scale = floor(scale * scr) / scr;
    self.lastScale = scale;
    CGFloat gw = gs.width * scale, gh = gs.height * scale;
    CGFloat gx = gameRect.origin.x + (gameRect.size.width - gw) / 2;
    CGFloat gy = gameRect.origin.y + (gameRect.size.height - gh) / 2;
    if (scr > 0) {
        /* frame 的 origin 与 size 必须都落在物理像素边界：
         * 仅对齐缩放倍率不够——origin 带小数（如导航栏底 95.67pt）时，
         * 合成器对 layer 做亚像素双线性混合，最近邻放大被污染，
         * 每行逻辑像素占的物理像素数不均 -> 文字横向碎裂、棋盘出 moiré 横纹 */
        gx = floor(gx * scr) / scr;
        gy = floor(gy * scr) / scr;
        gw = floor(gw * scr + 0.5) / scr;
        gh = floor(gh * scr + 0.5) / scr;
    }
    _gameView.frame = CGRectMake(gx, gy, gw, gh);
    if (self.padVisible) {
        _keyPad.hidden = NO;
        _keyPad.frame = CGRectMake(0, bottom - padH, vb.size.width, padH);
        /* 键盘内容（336 宽）居中 */
        UIView* content = [_keyPad.subviews firstObject];
        if (content && content.tag == 1001) {
            content.center = CGPointMake(_keyPad.bounds.size.width / 2, _keyPad.bounds.size.height / 2);
        }
    } else {
        _keyPad.frame = CGRectMake(0, vb.size.height, vb.size.width, padH);
    }

    /* OSD 定位：画面顶部居中 */
    if (_osdLabel) {
        [_osdLabel sizeToFit];
        CGRect of = _osdLabel.frame;
        of.size.width += 28;
        of.size.height = 30;
        _osdLabel.frame = CGRectMake(gameRect.origin.x + (gameRect.size.width - of.size.width) / 2,
                                     gameRect.origin.y + 10, of.size.width, of.size.height);
    }
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    [self layoutAll];
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }

/* 横竖屏自由旋转（倒屏除外） */
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskAllButUpsideDown;
}

/* ============ 虚拟键盘（分离式） ============ */

- (void)buildKeyPad {
    _keyPad = [[UIView alloc] initWithFrame:CGRectZero];
    _keyPad.backgroundColor = [UIColor colorWithWhite:0.12 alpha:1];

    /* 内容容器（336x224，layoutAll 中水平居中） */
    UIView* content = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 336, 224)];
    content.tag = 1001;
    content.userInteractionEnabled = YES;
    [_keyPad addSubview:content];

    /* 软键行（顶部）：左软键 -6 开菜单 / 右软键 -7 取消（Nokia S40 游戏必需） */
    struct { CGFloat x; int code; NSString* title; } softKeys[] = {
        {8,   K_SOFT1, @"左软键"},
        {172, K_SOFT2, @"右软键"},
    };
    for (int i = 0; i < 2; i++) {
        CGRect f = CGRectMake(softKeys[i].x, 8, 156, 36);
        [self addKeyButtonTo:content frame:f code:softKeys[i].code title:softKeys[i].title fontSize:15.0];
    }

    /* 方向十字（左）：▲ / ◀ ● ▶ / ▼ */
    struct { int col, row; int code; NSString* title; } padKeys[] = {
        {1, 0, K_UP,    @"▲"},
        {0, 1, K_LEFT,  @"◀"},
        {1, 1, K_FIRE,  @"●"},
        {2, 1, K_RIGHT, @"▶"},
        {1, 2, K_DOWN,  @"▼"},
    };
    CGFloat bs = 56.0;
    CGFloat ox = 8.0, oy = 52.0;
    for (int i = 0; i < 5; i++) {
        CGRect f = CGRectMake(ox + padKeys[i].col * bs, oy + padKeys[i].row * bs, bs - 4, bs - 4);
        [self addKeyButtonTo:content frame:f code:padKeys[i].code title:padKeys[i].title];
    }

    /* 数字块（右）：1 3 / 7 9 / * 0 */
    struct { int col, row; int code; NSString* title; } numKeys[] = {
        {0, 0, K_A,    @"1"},
        {1, 0, K_B,    @"3"},
        {0, 1, K_C,    @"7"},
        {1, 1, K_D,    @"9"},
        {0, 2, K_STAR, @"✱"},
        {1, 2, K_ZERO, @"0"},
    };
    CGFloat nx = 336.0 - 8.0 - 2 * bs, ny = oy;
    for (int i = 0; i < 6; i++) {
        CGRect f = CGRectMake(nx + numKeys[i].col * bs, ny + numKeys[i].row * bs, bs - 4, bs - 4);
        [self addKeyButtonTo:content frame:f code:numKeys[i].code title:numKeys[i].title];
    }
}

- (void)addKeyButtonTo:(UIView*)parent frame:(CGRect)frame code:(int)code title:(NSString*)title {
    [self addKeyButtonTo:parent frame:frame code:code title:title fontSize:22.0];
}

- (void)addKeyButtonTo:(UIView*)parent frame:(CGRect)frame code:(int)code title:(NSString*)title
              fontSize:(CGFloat)fontSize {
    KeyPadButton* btn = [KeyPadButton buttonWithType:UIButtonTypeSystem];
    btn.frame = frame;
    btn.keyCode = code;
    [btn setTitle:title forState:UIControlStateNormal];
    [btn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    btn.titleLabel.font = [UIFont boldSystemFontOfSize:fontSize];
    btn.backgroundColor = [UIColor colorWithWhite:0.22 alpha:1];
    btn.layer.cornerRadius = 10;
    [btn addTarget:self action:@selector(keyDown:) forControlEvents:UIControlEventTouchDown];
    [btn addTarget:self action:@selector(keyUp:)
       forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside
                  | UIControlEventTouchCancel];
    [parent addSubview:btn];
}

- (void)keyDown:(KeyPadButton*)sender {
    sender.backgroundColor = [UIColor colorWithWhite:0.45 alpha:1];   /* 按下高亮：确认触摸已生效 */
    [self keyPress:sender.keyCode down:YES];
}
- (void)keyUp:(KeyPadButton*)sender {
    sender.backgroundColor = [UIColor colorWithWhite:0.22 alpha:1];
    [self keyPress:sender.keyCode down:NO];
}

- (void)keyPress:(int)code down:(BOOL)down {
    /* 允许 Nokia 软键等负键码（-1..-31），其余仍限 1..255 */
    if (self.paused || code == 0 || code >= 256 || code < -31) return;
    BOOL soft = (code < 0);
    if (down) {
        if (!soft) {
            gKeyHeld[code] = CACurrentMediaTime();
            gKeyLastRep[code] = 0;
        }
        j2meKey(code, 1);
    } else {
        if (!soft) gKeyHeld[code] = 0;
        j2meKey(code, 0);
    }
}

/* ============ OSD 提示 ============ */

- (void)showOsd:(NSString*)text {
    if (!_osdLabel) return;
    _osdLabel.text = text;
    _osdLabel.hidden = NO;
    _osdHiding = NO;
    [UIView animateWithDuration:0.15 animations:^{
        self->_osdLabel.alpha = 1;
    }];
    _osdUntil = CACurrentMediaTime() + 2.0;
}

- (void)osdTick:(double)now {
    if (!_osdLabel || _osdLabel.hidden || _osdHiding) return;
    if (now > _osdUntil) {
        _osdHiding = YES;
        [UIView animateWithDuration:0.4 animations:^{
            self->_osdLabel.alpha = 0;
        } completion:^(BOOL f) {
            self->_osdLabel.hidden = YES;
            self->_osdHiding = NO;
        }];
    }
}

/* ============ 缩放档位 ============ */

- (void)cycleScale:(id)sender {
    self.scaleMode = (self.scaleMode + 1) % 4;
    [self layoutAll];
    [self showOsd:[NSString stringWithFormat:@"缩放：%.1fx", self.lastScale]];
}

/* ============ 帧驱动 ============ */

- (void)tick:(CADisplayLink*)link {
    double now = CACurrentMediaTime();
    /* 按住重复（0.45s 后每 0.12s 一次） */
    for (int c = 0; c < 256; c++) {
        if (gKeyHeld[c] > 0 && now - gKeyHeld[c] > 0.45 &&
            (gKeyLastRep[c] == 0 || now - gKeyLastRep[c] > 0.12)) {
            gKeyLastRep[c] = now;
            j2meKey(c, 2);
        }
    }
    [self osdTick:now];
    if (self.paused) return;

    int w = 0, h = 0;
    const uint32_t* px = j2meFrame(&w, &h);
    if (!px) {
        [self stopLink];
        NSString* err = [NSString stringWithUTF8String:j2meLastError()];
        [self fatalAlert:err.length ? err : @"游戏已结束"];
        return;
    }
    if (w > 0 && h > 0) [self.gameView updatePixels:px w:w h:h];
}

- (void)stopLink {
    [_displayLink invalidate];
    _displayLink = nil;
}

/* ============ 暂停 / 键盘切换 / 缩放 / 音量 / 截图 / 重启 / 退出 ============ */

- (void)togglePad:(id)sender {
    self.padVisible = !self.padVisible;
    self.keyPad.hidden = NO;
    [UIView animateWithDuration:0.25 animations:^{
        [self layoutAll];
    }];
}

- (void)showPauseMenu:(id)sender {
    [self setPaused:YES];
    /* 标题带版本号：用户可自查实际安装的版本 */
    NSString* ver = [NSBundle mainBundle].infoDictionary[@"CFBundleShortVersionString"];
    NSString* bld = [NSBundle mainBundle].infoDictionary[@"CFBundleVersion"];
    UIAlertController* sheet = [UIAlertController
        alertControllerWithTitle:[NSString stringWithFormat:@"Java模拟器 v%@ (build %@)", ver, bld]
        message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    [sheet addAction:[UIAlertAction actionWithTitle:@"继续游戏" style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) { [self setPaused:NO]; }]];

    NSString* padTitle = self.padVisible ? @"隐藏虚拟键盘" : @"显示虚拟键盘";
    [sheet addAction:[UIAlertAction actionWithTitle:padTitle style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) {
            self.padVisible = !self.padVisible;
            self.keyPad.hidden = NO;
            [UIView animateWithDuration:0.25 animations:^{ [self layoutAll]; }];
            [self setPaused:NO];
        }]];

    [sheet addAction:[UIAlertAction actionWithTitle:@"切换缩放档位" style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) {
            [self cycleScale:nil];
            [self setPaused:NO];
        }]];

    [sheet addAction:[UIAlertAction
        actionWithTitle:[NSString stringWithFormat:@"音量调节（当前 %d%%）", j2meVolume()]
        style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) { [self showVolumeDialog]; }]];

    [sheet addAction:[UIAlertAction actionWithTitle:@"截图保存到相册" style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) {
            [self doScreenshot];
            [self setPaused:NO];
        }]];

    [sheet addAction:[UIAlertAction actionWithTitle:@"重启游戏" style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) { [self restartGame]; }]];

    [sheet addAction:[UIAlertAction actionWithTitle:@"返回游戏库" style:UIAlertActionStyleDestructive
        handler:^(UIAlertAction* a) { [self quitGame]; }]];

    [sheet addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel
        handler:^(UIAlertAction* a) { [self setPaused:NO]; }]];

    /* iPad 弹出锚点 */
    if (sheet.popoverPresentationController) {
        if ([sender isKindOfClass:[UIBarButtonItem class]]) {
            sheet.popoverPresentationController.barButtonItem = (UIBarButtonItem*)sender;
        } else {
            sheet.popoverPresentationController.sourceView = self.view;
            sheet.popoverPresentationController.sourceRect =
                CGRectMake(self.view.bounds.size.width / 2, 60, 1, 1);
        }
    }
    [self presentViewController:sheet animated:YES completion:nil];
}

- (void)setPaused:(BOOL)p {
    if (_paused == p) return;
    _paused = p;
    if (p) {
        j2mePause();
        for (int c = 0; c < 256; c++) {
            if (gKeyHeld[c] > 0) { gKeyHeld[c] = 0; j2meKey(c, 0); }
        }
    } else {
        j2meResume();
    }
}

/* ============ 音量弹窗（自定义卡片，实时生效） ============ */

- (void)showVolumeDialog {
    UIView* overlay = [[UIView alloc] initWithFrame:self.view.bounds];
    overlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    overlay.backgroundColor = [UIColor colorWithWhite:0 alpha:0.6];
    overlay.tag = 9001;

    UIView* card = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 300, 168)];
    card.center = CGPointMake(overlay.bounds.size.width / 2, overlay.bounds.size.height / 2);
    card.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin
                          | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
    card.backgroundColor = [UIColor colorWithWhite:0.14 alpha:1];
    card.layer.cornerRadius = 18;

    UILabel* title = [[UILabel alloc] initWithFrame:CGRectMake(0, 18, 300, 24)];
    title.text = @"音量";
    title.textColor = [UIColor whiteColor];
    title.font = [UIFont boldSystemFontOfSize:17];
    title.textAlignment = NSTextAlignmentCenter;
    [card addSubview:title];

    UILabel* value = [[UILabel alloc] initWithFrame:CGRectMake(0, 48, 300, 22)];
    value.text = [NSString stringWithFormat:@"%d%%", j2meVolume()];
    value.textColor = [UIColor colorWithRed:0.4 green:0.8 blue:1 alpha:1];
    value.font = [UIFont boldSystemFontOfSize:16];
    value.textAlignment = NSTextAlignmentCenter;
    value.tag = 9002;
    [card addSubview:value];

    UISlider* slider = [[UISlider alloc] initWithFrame:CGRectMake(28, 82, 300 - 56, 34)];
    slider.minimumValue = 0;
    slider.maximumValue = 100;
    slider.value = j2meVolume();
    [slider addTarget:self action:@selector(volumeChanged:)
       forControlEvents:UIControlEventValueChanged];
    [card addSubview:slider];

    UIButton* done = [UIButton buttonWithType:UIButtonTypeSystem];
    done.frame = CGRectMake(70, 122, 160, 36);
    done.tintColor = [UIColor whiteColor];
    done.backgroundColor = [UIColor colorWithWhite:0.28 alpha:1];
    done.layer.cornerRadius = 10;
    [done setTitle:@"完成" forState:UIControlStateNormal];
    [done addTarget:self action:@selector(closeVolumeDialog)
       forControlEvents:UIControlEventTouchUpInside];
    [card addSubview:done];

    [overlay addSubview:card];
    [self.view addSubview:overlay];
}

- (void)volumeChanged:(UISlider*)s {
    int v = (int)(s.value + 0.5);
    j2meSetVolume(v);
    UILabel* value = (UILabel*)[self.view viewWithTag:9002];
    if (value) value.text = [NSString stringWithFormat:@"%d%%", v];
}

- (void)closeVolumeDialog {
    [[self.view viewWithTag:9001] removeFromSuperview];
    [self setPaused:NO];
}

/* ============ 截图（当前帧 -> 相册） ============ */

- (void)doScreenshot {
    CGImageRef img = (__bridge CGImageRef)self.gameView.layer.contents;
    if (!img) {
        [self showOsd:@"画面尚未就绪，稍后再试"];
        return;
    }
    UIImage* ui = [UIImage imageWithCGImage:img];
    UIImageWriteToSavedPhotosAlbum(ui, self,
        @selector(image:didFinishSavingWithError:contextInfo:), nil);
}

- (void)image:(UIImage*)image didFinishSavingWithError:(NSError*)error contextInfo:(void*)c {
    if (error) [self showOsd:@"保存失败：请在系统设置中允许写入相册"];
    else       [self showOsd:@"截图已保存到相册"];
}

/* ============ 重启 / 退出 ============ */

- (void)restartGame {
    j2meDestroy();
    memset(gKeyHeld, 0, sizeof(gKeyHeld));
    NSString* midpPath = [[NSBundle mainBundle] pathForResource:@"midp" ofType:@"jar"];
    int rc = j2meStart(midpPath.UTF8String, self.jarPath.UTF8String,
                       self.rmsDir.UTF8String, 240, 320);
    if (rc != 0) {
        [self fatalAlert:[NSString stringWithUTF8String:j2meLastError()]];
        return;
    }
    const char* mn = j2meMidletName();
    if (mn && mn[0]) self.navigationItem.title = [NSString stringWithUTF8String:mn];
    _paused = NO;
    if (!_displayLink) {
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
        [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    }
    [self showOsd:@"已重启"];
}

- (void)quitGame {
    [self stopLink];
    j2meDestroy();
    [self.navigationController popViewControllerAnimated:YES];
}

- (void)fatalAlert:(NSString*)msg {
    [self stopLink];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"无法运行"
        message:msg preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"返回" style:UIAlertActionStyleDefault
        handler:^(UIAlertAction* a) {
            j2meDestroy();
            [self.navigationController popViewControllerAnimated:YES];
        }]];
    [self presentViewController:alert animated:YES completion:nil];
}

/* ============ e2e：虚拟键盘自检 ============ */
- (void)collectKeyButtons:(UIView*)root into:(NSMutableArray*)out {
    for (UIView* v in root.subviews) {
        if ([v isKindOfClass:[KeyPadButton class]]) [out addObject:v];
        [self collectKeyButtons:v into:out];
    }
}

- (void)startE2EUICheckIfNeeded {
    NSArray* args = [[NSProcessInfo processInfo] arguments];
    if (![args containsObject:@"--e2e-uicheck"]) return;
    __weak typeof(self) ws = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3.0 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{ [ws runE2EUICheck]; });
}

/* 自检两项：
 * 1) 命中测试——每个按键中心点必须 hitTest 命中自身（排查被遮挡/不可交互）
 * 2) target-action——向真实按钮发送 TouchDown/Up，走完整 keyPress->j2meKey 链路
 * 结果写 Documents/uicheck.txt，CI 读取断言 */
- (void)runE2EUICheck {
    NSMutableString* log = [NSMutableString string];
    NSMutableArray* btns = [NSMutableArray array];
    [self collectKeyButtons:self.keyPad into:btns];
    UIWindow* win = self.view.window;
    [log appendFormat:@"buttons=%lu\n", (unsigned long)btns.count];
    int hitBad = 0;
    for (KeyPadButton* b in btns) {
        CGPoint c = [b convertPoint:CGPointMake(CGRectGetWidth(b.bounds) / 2,
                                               CGRectGetHeight(b.bounds) / 2) toView:win];
        UIView* hit = [win hitTest:c withEvent:nil];
        BOOL ok = (hit == b) || (hit && [hit isDescendantOfView:b]);
        if (!ok) hitBad++;
        [log appendFormat:@"hit code=%d ok=%d hit=%@\n",
            b.keyCode, (int)ok, hit ? NSStringFromClass(hit.class) : @"nil"];
    }
    /* 事件链路：4/6/5（左右+确认）各一次按下抬起 */
    int before = j2meKeyDispatched();
    for (NSNumber* n in @[@(K_LEFT), @(K_RIGHT), @(K_FIRE)]) {
        int code = n.intValue;
        KeyPadButton* t = nil;
        for (KeyPadButton* b in btns) if (b.keyCode == code) { t = b; break; }
        if (!t) { [log appendFormat:@"act code=%d MISSING\n", code]; continue; }
        [t sendActionsForControlEvents:UIControlEventTouchDown];
        [t sendActionsForControlEvents:UIControlEventTouchUpInside];
        [log appendFormat:@"act code=%d sent\n", code];
    }
    [log appendFormat:@"hitBad=%d\n", hitBad];
    [log appendFormat:@"dispatch_before=%d\n", before];
    NSString* dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
    NSString* path = [dir stringByAppendingPathComponent:@"uicheck.txt"];
    /* 等 1 秒让帧驱动跑完冲刷，再写入派发增量 */
    __weak typeof(self) ws = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        typeof(self) s = ws;
        if (!s) return;
        int after = j2meKeyDispatched();
        NSMutableString* out = [NSMutableString stringWithString:log];
        [out appendFormat:@"dispatch_after=%d\n", after];
        [out writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
    });
}

/* ============ 前后台 ============ */
- (void)onEnterBackground {
    self.pausedByBackground = YES;
    [self setPaused:YES];
}
- (void)onEnterForeground {
    /* 切后台导致的暂停：回前台自动恢复。
     * 此前不回自动恢复，回到游戏时画面静止、所有按键被 paused 拦截，
     * 表现为「按键完全没反应」且没有任何提示 */
    if (self.pausedByBackground) {
        self.pausedByBackground = NO;
        [self setPaused:NO];
        [self showOsd:@"已恢复"];
    }
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    if (self.isMovingFromParentViewController || self.isBeingDismissed) {
        [self stopLink];
        j2meDestroy();
    }
}

/* ============ 蓝牙手柄（GCController） ============ */

- (void)watchControllers {
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onControllerConnect:)
        name:GCControllerDidConnectNotification object:nil];
    for (GCController* c in [GCController controllers]) [self bindController:c];
}

- (void)onControllerConnect:(NSNotification*)n {
    GCController* c = n.object;
    if ([c isKindOfClass:[GCController class]]) [self bindController:c];
}

- (void)bindController:(GCController*)c {
    GCExtendedGamepad* gp = c.extendedGamepad;
    if (gp) {
        [self bindBtn:gp.dpad.up code:K_UP];
        [self bindBtn:gp.dpad.down code:K_DOWN];
        [self bindBtn:gp.dpad.left code:K_LEFT];
        [self bindBtn:gp.dpad.right code:K_RIGHT];
        [self bindBtn:gp.buttonA code:K_FIRE];
        [self bindBtn:gp.buttonB code:K_ZERO];
        [self bindBtn:gp.buttonX code:K_A];
        [self bindBtn:gp.buttonY code:K_B];
        [self bindBtn:gp.leftShoulder code:K_C];
        [self bindBtn:gp.rightShoulder code:K_D];
        [self bindBtn:gp.leftTrigger code:K_STAR];
        [self bindBtn:gp.rightTrigger code:K_POUND];
        __weak typeof(self) ws = self;
        gp.buttonMenu.pressedChangedHandler = ^(GCControllerButtonInput* b, float v, BOOL pressed) {
            if (pressed) dispatch_async(dispatch_get_main_queue(), ^{ [ws showPauseMenu:nil]; });
        };
    } else if (c.microGamepad) {
        GCMicroGamepad* mp = c.microGamepad;
        [self bindBtn:mp.dpad.up code:K_UP];
        [self bindBtn:mp.dpad.down code:K_DOWN];
        [self bindBtn:mp.dpad.left code:K_LEFT];
        [self bindBtn:mp.dpad.right code:K_RIGHT];
        [self bindBtn:mp.buttonA code:K_FIRE];
        [self bindBtn:mp.buttonX code:K_A];
    }
}

- (void)bindBtn:(GCControllerButtonInput*)btn code:(int)code {
    __weak typeof(self) ws = self;
    btn.pressedChangedHandler = ^(GCControllerButtonInput* b, float value, BOOL pressed) {
        dispatch_async(dispatch_get_main_queue(), ^{ [ws keyPress:code down:pressed]; });
    };
}

@end
