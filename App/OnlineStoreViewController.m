/* OnlineStoreViewController.m - 在线游戏仓库主页
 * 结构：顶部分类区（可钻取）+ 游戏分页列表（每页 24，滚到底自动加载下一页）
 *      搜索框回车即按关键词全库搜索（忽略当前分类）；
 *      下拉刷新重载第一页与分类。数据源 java.iwkk.cn。
 */
#import "OnlineStoreViewController.h"
#import "RetroBox.h"
#import "GameDetailViewController.h"

/* ============ 游戏行自定义 Cell（图标异步 + 两行文字） ============ */

@interface GameCell : UITableViewCell
@property (nonatomic, strong) UIImageView* iconView;
@property (nonatomic, strong) UILabel* nameL;
@property (nonatomic, strong) UILabel* subL;
@property (nonatomic, copy)   NSString* iconURL;   /* 当前行正在显示的图标地址 */
- (void)fillGame:(NSDictionary*)g;
- (void)fillCategory:(NSDictionary*)c;
@end

@implementation GameCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString*)rid {
    if ((self = [super initWithStyle:UITableViewCellStyleDefault reuseIdentifier:rid])) {
        _iconView = [[UIImageView alloc] init];
        _iconView.contentMode = UIViewContentModeScaleAspectFill;
        _iconView.clipsToBounds = YES;
        _iconView.layer.cornerRadius = 10;
        _iconView.backgroundColor = [UIColor colorWithWhite:0.18 alpha:1];
        [self.contentView addSubview:_iconView];
        _nameL = [[UILabel alloc] init];
        _nameL.font = [UIFont boldSystemFontOfSize:16];
        _nameL.textColor = [UIColor whiteColor];
        _nameL.lineBreakMode = NSLineBreakByTruncatingTail;
        [self.contentView addSubview:_nameL];
        _subL = [[UILabel alloc] init];
        _subL.font = [UIFont systemFontOfSize:12];
        _subL.textColor = [UIColor colorWithWhite:0.58 alpha:1];
        _subL.lineBreakMode = NSLineBreakByTruncatingMiddle;
        [self.contentView addSubview:_subL];
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CGFloat h = self.contentView.bounds.size.height;
    CGFloat ic = 46;
    _iconView.frame = CGRectMake(12, (h - ic) / 2, ic, ic);
    CGFloat x = 12 + ic + 12;
    CGFloat w = self.contentView.bounds.size.width - x - 30;
    _nameL.frame = CGRectMake(x, h / 2 - 22, w, 20);
    _subL.frame = CGRectMake(x, h / 2 + 2, w, 16);
}

/* 抽象方块占位图标（与本地库同风格：蓝青渐变 + "J2"） */
+ (UIImage*)placeholder {
    static UIImage* img = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        CGSize s = CGSizeMake(92, 92);
        UIGraphicsImageRenderer* r = [[UIGraphicsImageRenderer alloc] initWithSize:s];
        img = [r imageWithActions:^(UIGraphicsImageRendererContext* ctx) {
            CGRect b = CGRectMake(0, 0, s.width, s.height);
            UIBezierPath* clip = [UIBezierPath bezierPathWithRoundedRect:b cornerRadius:22];
            [clip addClip];
            NSArray* colors = @[(id)[UIColor colorWithRed:0.14 green:0.30 blue:0.85 alpha:1].CGColor,
                                 (id)[UIColor colorWithRed:0.05 green:0.72 blue:0.72 alpha:1].CGColor];
            CGGradientRef grad = CGGradientCreateWithColors(nil, (__bridge CFArrayRef)colors, NULL);
            CGContextDrawLinearGradient(ctx.CGContext, grad, CGPointMake(0, 0),
                CGPointMake(0, s.height), 0);
            CGGradientRelease(grad);
            NSMutableParagraphStyle* ps = [[NSMutableParagraphStyle alloc] init];
            ps.alignment = NSTextAlignmentCenter;
            NSDictionary* at = @{NSFontAttributeName: [UIFont boldSystemFontOfSize:34],
                                 NSForegroundColorAttributeName: [UIColor whiteColor],
                                 NSParagraphStyleAttributeName: ps};
            [@"J2" drawInRect:CGRectMake(0, 24, s.width, 44) withAttributes:at];
        }];
    });
    return img;
}

- (void)setIconFromURL:(NSString*)url {
    self.iconURL = url;
    self.iconView.image = [GameCell placeholder];
    if (!url.length) return;
    UIImage* hit = [RetroBox cachedIcon:url];
    if (hit) {
        self.iconView.image = hit;
        return;
    }
    __weak GameCell* ws = self;
    [RetroBox loadIcon:url completion:^(UIImage* img) {
        GameCell* s = ws;
        if (s && img && [s.iconURL isEqualToString:url]) s.iconView.image = img;
    }];
}

- (void)fillGame:(NSDictionary*)g {
    self.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    self.nameL.text = g[@"name"];
    /* 副标题：厂商 · 机型/分辨率变体（取 fileName 末段） */
    NSString* variant = [[(g[@"fileName"] ?: @"") lastPathComponent]
        stringByDeletingPathExtension];
    self.subL.text = [NSString stringWithFormat:@"%@ · %@",
        g[@"vendor"] ?: @"未知厂商", variant.length ? variant : @"通用版"];
    [self setIconFromURL:g[@"iconUrl"]];
}

- (void)fillCategory:(NSDictionary*)c {
    self.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    self.iconView.image = [GameCell placeholder];
    self.nameL.text = c[@"name"];
    self.subL.text = [NSString stringWithFormat:@"%ld 个游戏%@",
        (long)[c[@"count"] integerValue],
        [c[@"hasChildren"] boolValue] ? @" · 含子分类" : @""];
    self.iconURL = nil;
}

@end

/* ============ 主控制器 ============ */

@interface OnlineStoreViewController () <UISearchBarDelegate>
@property (nonatomic, copy) NSString* categoryPath;
@property (nonatomic, strong) NSMutableArray<NSDictionary*>* categories;
@property (nonatomic, strong) NSMutableArray<NSDictionary*>* games;
@property (nonatomic, assign) NSInteger page;
@property (nonatomic, assign) NSInteger totalPages;
@property (nonatomic, assign) NSInteger total;
@property (nonatomic, assign) BOOL loading;
@property (nonatomic, copy)   NSString* keyword;
@property (nonatomic, strong) UISearchBar* searchBar;
@property (nonatomic, strong) UILabel* footLabel;
@property (nonatomic, strong) UIRefreshControl* refresh;
@end

@implementation OnlineStoreViewController

- (instancetype)initWithCategoryPath:(NSString*)path title:(NSString*)title {
    self = [super initWithStyle:UITableViewStylePlain];
    if (self) {
        _categoryPath = [path copy];
        _page = 1;
        self.title = title.length ? title : @"在线游戏仓库";
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.tableView.rowHeight = 62;
    self.tableView.separatorInset = UIEdgeInsetsMake(0, 72, 0, 0);

    /* 搜索框 */
    _searchBar = [[UISearchBar alloc] initWithFrame:CGRectMake(0, 0,
        self.view.bounds.size.width, 52)];
    _searchBar.placeholder = @"搜索游戏名，如：三国";
    _searchBar.delegate = self;
    _searchBar.showsCancelButton = YES;
    _searchBar.barStyle = UIBarStyleBlack;
    self.tableView.tableHeaderView = _searchBar;

    /* 下拉刷新 */
    _refresh = [[UIRefreshControl alloc] init];
    _refresh.tintColor = [UIColor colorWithWhite:0.7 alpha:1];
    [_refresh addTarget:self action:@selector(reloadAll)
        forControlEvents:UIControlEventValueChanged];
    self.refreshControl = _refresh;

    /* 底部加载状态 */
    _footLabel = [[UILabel alloc] initWithFrame:CGRectMake(0, 0,
        self.view.bounds.size.width, 44)];
    _footLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    _footLabel.textAlignment = NSTextAlignmentCenter;
    _footLabel.font = [UIFont systemFontOfSize:13];
    _footLabel.textColor = [UIColor colorWithWhite:0.5 alpha:1];
    _footLabel.text = @"加载中…";
    self.tableView.tableFooterView = _footLabel;

    [self reloadAll];
}

- (void)reloadAll {
    [self loadCategories];
    [self fetchPage:1];
}

/* ============ 数据加载 ============ */

- (void)loadCategories {
    NSDictionary* q = self.categoryPath.length ? @{@"parent": self.categoryPath} : nil;
    __weak OnlineStoreViewController* ws = self;
    [RetroBox getJSON:@"/api/categories" query:q completion:^(id json, NSString* error) {
        OnlineStoreViewController* s = ws;
        if (!s) return;
        if (error || ![json isKindOfClass:[NSDictionary class]]) return; /* 分类失败不打断游戏列表 */
        NSArray* arr = json[@"categories"];
        if (![arr isKindOfClass:[NSArray class]]) arr = @[];
        s.categories = [NSMutableArray arrayWithArray:arr];
        [s.tableView reloadData];
    }];
}

- (void)fetchPage:(NSInteger)p {
    if (self.loading) return;
    self.loading = YES;
    if (p == 1) {
        self.games = [NSMutableArray array];
        self.page = 1;
        self.footLabel.text = @"加载中…";
        [self.tableView reloadData];
    }
    NSMutableDictionary* q = [NSMutableDictionary dictionary];
    q[@"page"] = @(p);
    q[@"pageSize"] = @24;
    if (self.keyword.length) q[@"keyword"] = self.keyword;
    else if (self.categoryPath.length) q[@"category"] = self.categoryPath;
    __weak OnlineStoreViewController* ws = self;
    [RetroBox getJSON:@"/api/games" query:q completion:^(id json, NSString* error) {
        OnlineStoreViewController* s = ws;
        if (!s) return;
        s.loading = NO;
        [s.refresh endRefreshing];
        if (error || ![json isKindOfClass:[NSDictionary class]]) {
            s.footLabel.text = @"加载失败，下拉重试";
            if (p == 1 && !s.games.count) {
                UIAlertController* a = [UIAlertController
                    alertControllerWithTitle:@"加载失败"
                    message:error ?: @"网络错误"
                    preferredStyle:UIAlertControllerStyleAlert];
                [a addAction:[UIAlertAction actionWithTitle:@"好"
                    style:UIAlertActionStyleDefault handler:nil]];
                [s presentViewController:a animated:YES completion:nil];
            }
            return;
        }
        NSArray* arr = json[@"games"];
        if (![arr isKindOfClass:[NSArray class]]) arr = @[];
        if (!s.games) s.games = [NSMutableArray array];
        [s.games addObjectsFromArray:arr];
        s.page = p;
        s.total = [json[@"total"] integerValue];
        s.totalPages = [json[@"totalPages"] integerValue];
        s.footLabel.text = (s.page < s.totalPages)
            ? @"上滑加载更多" : @"没有更多了";
        [s.tableView reloadData];
    }];
}

/* ============ 搜索 ============ */

- (void)searchBarSearchButtonClicked:(UISearchBar*)sb {
    [sb resignFirstResponder];
    self.keyword = [sb.text stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    [self fetchPage:1];
}

- (void)searchBarCancelButtonClicked:(UISearchBar*)sb {
    [sb resignFirstResponder];
    if (self.keyword.length) {
        self.keyword = nil;
        sb.text = nil;
        [self fetchPage:1];
    }
}

/* ============ UITableView ============ */

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tv {
    if (self.keyword.length) return 1;                       /* 搜索模式：只有游戏 */
    return self.categories.count ? 2 : 1;
}

- (NSInteger)tableView:(UITableView*)tv numberOfRowsInSection:(NSInteger)section {
    BOOL showCats = !self.keyword.length && self.categories.count;
    if (section == 0) return showCats ? self.categories.count : self.games.count;
    return self.games.count;
}

- (NSString*)tableView:(UITableView*)tv titleForHeaderInSection:(NSInteger)section {
    BOOL showCats = !self.keyword.length && self.categories.count;
    if (showCats && section == 0) return @"分类";
    if (self.keyword.length)
        return [NSString stringWithFormat:@"搜索“%@” · 共 %ld 个",
            self.keyword, (long)self.total];
    return [NSString stringWithFormat:@"全部游戏 · 共 %ld 个", (long)self.total];
}

- (UITableViewCell*)tableView:(UITableView*)tv cellForRowAtIndexPath:(NSIndexPath*)ip {
    GameCell* cell = [tv dequeueReusableCellWithIdentifier:@"g"];
    if (!cell) cell = [[GameCell alloc] initWithStyle:UITableViewCellStyleDefault
                                     reuseIdentifier:@"g"];
    BOOL showCats = !self.keyword.length && self.categories.count;
    if (showCats && ip.section == 0) {
        [cell fillCategory:self.categories[ip.row]];
    } else {
        [cell fillGame:self.games[ip.row]];
    }
    return cell;
}

- (void)tableView:(UITableView*)tv willDisplayCell:(UITableViewCell*)cell
    forRowAtIndexPath:(NSIndexPath*)ip {
    /* 滚到最后一行 → 自动取下一页 */
    BOOL showCats = !self.keyword.length && self.categories.count;
    NSInteger gamesSection = showCats ? 1 : 0;
    if (ip.section == gamesSection
        && self.games.count
        && ip.row == self.games.count - 1
        && self.page < self.totalPages && !self.loading) {
        self.footLabel.text = @"加载中…";
        [self fetchPage:self.page + 1];
    }
}

- (void)tableView:(UITableView*)tv didSelectRowAtIndexPath:(NSIndexPath*)ip {
    [tv deselectRowAtIndexPath:ip animated:YES];
    BOOL showCats = !self.keyword.length && self.categories.count;
    if (showCats && ip.section == 0) {
        NSDictionary* c = self.categories[ip.row];
        [self.searchBar resignFirstResponder];
        OnlineStoreViewController* sub = [[OnlineStoreViewController alloc]
            initWithCategoryPath:c[@"path"] title:c[@"name"]];
        [self.navigationController pushViewController:sub animated:YES];
    } else {
        NSDictionary* g = self.games[ip.row];
        GameDetailViewController* d = [[GameDetailViewController alloc] initWithGame:g];
        [self.navigationController pushViewController:d animated:YES];
    }
}

@end
