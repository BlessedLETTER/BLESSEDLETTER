# 笺（ZSchedule）· 开发进展与交接

> 用途：开新对话继续开发时先读这份，避免重复劳动、避免踩已经踩过的坑。
> 最后更新：2026-10-17（v1.9.1：便笺墙卡片上的照片也改成 v1.9 那套——完整显示 / 四角缩放 / 拖动只改位置）。
> 项目目录：`C:\Users\Crychic\.zcode\workspace\default\ZSchedule`

---

## 0. 现状速览

纯 C + Win32 API（Unicode）写的便笺式桌面日程软件，单文件 exe、绿色免安装。
功能主体**全部完成且编译零 error / 零 warning**：

- 最新一轮（v1.9.4，复制/粘贴便笺 + 关于页，见 §3-AF）：
  - **便笺墙复制 / 粘贴**：工具栏「复制 / 粘贴」、编辑菜单、右键菜单、`Ctrl+C`/`Ctrl+V`（`Ctrl+D` 旧别名）；
    先「新开一页」再粘贴 → 新便笺落在新页；标题/正文/附件/照片设置整份照搬，新 id；空剪贴板置灰。
  - 搜索框里的 `Ctrl+C`/`Ctrl+V` 仍是文字复制粘贴，不会被日程命令截走。
  - **关于**：显示版本 `v1.9.4`、作者「祝筱笺」、项目主页 GitHub 链接（可点击）；
    版本/作者/链接集中在 `APP_VERSION` / `APP_AUTHOR` / `APP_URL`。
  - 顺带修好换页选中回跳、粘贴页归属不落盘、启动页恢复这三个页相关问题。
- 上一轮（v1.9.3，加急修正文滚动/照片放大，见 §3-AE）：
  - **滚轮滚到最底，正文不再消失**：布局与绘制各减了一次滚动量（正文 2×、照片 1×），
    现在**只减一次**；裁剪固定在内容视口（日期行顶/正文起点）到底边，最后一行正好贴底。
  - **照片放大不再挤掉正文**：只要有正文就永远留一行（照片拖到最大也一样），
    其余正文滚轮往下就能看到；拖照片本体也停在正文最后一行上方。
  - **滚动内容统一裁在视口里**：日期/倒计时/照片/正文一起滚，不会压住标题；滚动条与悬停按钮固定。
  - **便笺墙卡片同一口径**（双倍滚动 + 预留一行都修了）。
- 上一轮（v1.9.2，加急修照片，见 §3-AD）：
  - **照片不再随指针离开而消失**：以前“浮于文字上方”那层照片要 `imgHotIdx >= 0` 才画，
    v1.9.1 的 `WM_MOUSELEAVE` 把悬停清成 -1 → 照片整张不画。现在**无条件绘制**。
  - **悬停不再进编辑**：只有**单击选中**后才画虚线框 + 四角手柄；悬停只改光标。
  - **右键退出照片编辑模式**（同时照旧弹便笺菜单，`Esc` 关掉）；单击别处也退出；双击仍进编辑窗。
  - **便笺墙卡片同一口径**；顺带删掉布局里“最小宽/高撑开框”的钳制，
    照片框从此**贴住照片本身**（不再有左右/上下的一圈无效空白，选中框也不再比照片大一圈）。
- 上一轮（v1.9.1，便笺墙卡片照片补齐，见 §3-AC）：
  - 墙上卡片与独立便笺统一：**照片按自身比例完整显示（contain）**、框与照片同比例（无框内白边/无效空白）；
    没有正文不留高度、调过大小时用满卡片正文区。
  - **四个顶角两维同比例缩放**（悬停出角方块，斜向光标；边中段不再是手柄）；
    **Ctrl+滚轮不再缩放照片**；**拖动只改位置、尺寸不变**（顺带修了拖动夹紧会改高度的老毛病）。
- 上一轮（v1.9，独立便笺照片交互重做，见 §3-AB）：
  - **照片按自身比例完整显示（contain）**：不再被便笺形状裁掉；框跟着照片比例走，框里没有无效空白；
    便笺没有正文时不再给正文留高度；手动调过大小后照片区用满正文区（扁便笺也能放下竖图）。
  - **四个顶角双向缩放**：拖任意角 = 宽高同时按比例变；只认顶角，边的中段不再当手柄（和拖动彻底分开）；
    角上给斜向缩放光标、照片本体给移动光标。
  - **单击选中拖动 / 双击进编辑窗**：单击选中照片后拖动只改位置、尺寸不变；双击打开这条例程的编辑窗。
- 上一轮（v1.8）：
  - **迁移功能**：文件 → 数据保存位置 →「迁移：在桌面生成数据拷贝」→ 桌面出现
    `笺数据迁移_YYYYMMDD_HHMMSS\`（主数据 + 便笺状态 + 便笺墙 + 历史版本 + 迁移说明.txt），复制完自动打开；
    同一秒重复执行自动加 `_2`（见 §3-AA）。
  - **顺带修好一个老 bug**：`app_data_copy_to()` 复制历史目录时源路径漏了 `zsched_history\` 一层，
    所以以前「备份当前数据」「迁移」「切换数据目录」拷出来的历史目录一直是空的（见 §3-AA）。
- 上一轮（v1.7，两条修正）：
  - **课表接力时间点**：下课先不关窗，等**原课程结束 + 5 秒**再把同一个窗口整个换成下一节
    （连堂换正在上的那节、有课间就换下一节并显示「距上课」、今天没课就关窗）（见 §3-Z）。
  - **只勾「提示音」也有手动关闭**：响铃时弹一个带 **「停止铃声」** 按钮的小窗，点按钮/点窗即停，
    声音播完小窗自动关；勾了便笺弹窗的提醒不弹小窗（点弹窗就停）（见 §3-Z）。
- 上一轮（v1.6，四条）：
  - **铃声内置播放**：新模块 `sound.c`（Media Foundation → MCI → 系统程序三级兜底），
    **mp3 / flac 直接在程序里响**，不再拉起外部播放器；点提醒窗或 30 秒自动关就停铃（见 §3-V）。
  - **便笺容量上限移除 + 便笺墙翻页**：便笺窗口改动态数组（不再 64 上限）、启动恢复全部（不再 24 张，
    实测 80 张开 / 关 / 恢复都正常）；墙底页码栏 `‹ 第 n / m 页 › ＋ 新一页`，本页满时按钮变朱红
    「本页已满 · 新开一页」；每张便笺记 `page=`，当前页/总页数记 `viewpage=`/`pages=`（见 §3-W）。
  - **表格关键词搜索**：表格视图顶部搜索框，边打边筛（标题/正文/链接），状态栏显示命中数，Esc 清空、Ctrl+F 聚焦（见 §3-X）。
  - **后台运行**：常驻托盘图标，文件菜单「后台运行」把主窗口收进托盘（任务栏按钮消失），
    便笺与提醒照常工作，托盘左键切换显示（见 §3-Y）。
- 上一轮（v1.5）：自选铃声（打开所选文件）+ CSV 课表导入（§3-T/§3-U）。
- 更早：v1.4「下一节」名称行、v1.3.2 三项修复、逐段文字颜色与书签形态等。
- 最新构建：`笺.exe` **743,863 字节**（2026-10-18 12:54），编译零 error / 零 warning；
  MD5 `3eb2c2bc7995995869ed1a6ee49e470c`；桌面 `C:\Users\Crychic\Desktop\笺.exe` 已同步（见 §7）。
- **版本现状**：当前 **v1.9.4**；已把它打包成独立备份目录 `..\ZSchedule_backup_v1.9.4_20261018\`
  （最新版源码 + 编译好的 `笺.exe` + `备份说明.md`），在 git 仓库之外、不受本项目影响。做新备份见 §6。
- **git 提交历史**：`d21455b1`（初版 v1.3.1）→ `a6d86a46` → `2091601f`（= v1.3.2）
  → `84355cde`/`384d04b4`（v1.4）→ `c98cf362`（= v1.5）→ `fe69d39a`（= v1.6）→ `4121d162`（= v1.7）→ `bb744616`（= v1.8）→ `2ccc4d37`（= v1.9）→ `a60ad60f`（= v1.9.1）→ `af0802ef`（= v1.9.2）→ `e7edb989` + `66569386`（= v1.9.3）→ **`efa01ff5`（= v1.9.4）**。
  本机**没装 git**，用 `py _tools\git_init.py "说明"` 提交（走 dulwich）；导出某次提交的文件树用 `py _tools\export_commit.py HEAD <目标目录>`。

---

## 1. 环境与构建

| 项 | 说明 |
| --- | --- |
| 工具链 | 项目旁便携工具链 `..\tools\w64devkit`（MinGW-w64 gcc 13.1）；`build.bat` 会自动把它加进 PATH，找不到 gcc 时退回 MSVC |
| 构建 | `build.bat`（GBK + **CRLF**）或 `mingw32-make`（`Makefile`，UTF-8） |
| 输出 | `笺.exe`（`build.bat` 的 `set OUT=笺.exe`；`Makefile` 的 `TARGET := 笺.exe`） |
| 资源 | `zsched.rc` → `zsched_res.o`（含 `1 RT_MANIFEST` + `1 ICON "app.ico"`），`windres` 编译 |
| 源码 | `main.c model.c image.c richtext.c weekview.c imgview.c stickies.c noteview.c paper.c ui_main.c ui_edit.c csvimport.c sound.c` |
| 链接库 | `comctl32 gdi32 comdlg32 shell32 msimg32 gdiplus ole32 shlwapi user32` + v1.6 新增 `winmm mfplat mf mfuuid`（铃声播放） |

**编译前必须 `taskkill /IM 笺.exe /F`**（exe 被占用时 gcc 链接会失败，且失败信息不直观）。

**最新版备份**：`..\ZSchedule_backup_v1.5_20261017\`（同级目录，独立于本工程）——
里面是**当前最新版**的完整源码 + 编译好的 `笺.exe`（629,353 字节，与工程/桌面字节相同）+ `备份说明.md`，
双击即可运行；它自己的 `build.bat` 也能独立编译（会自动用 `..\tools\w64devkit`）。做新备份的方法见 §6。

---

## 2. 数据文件（统一放在“数据目录”，默认 exe 同目录）

| 文件 | 内容 |
| --- | --- |
| `zsched_data.txt` | 主数据，UTF-16LE + BOM，格式版本 `ZScheduleData/4`（v1.5 只加了两行可选字段，版本号没变） |
| `zsched_notes.txt` | 桌面便笺窗口状态：位置 / 尺寸 / 颜色 / 外观 / 置顶（首行 `ttauto=` 是课表自动跟随开关） |
| `zsched_board.txt` | 便笺墙里每张便笺的 `x,y,w,h,pinned,hidden,page`；文件开头另有 `viewpage=`（上次看第几页）、`pages=`（一共几页，空页也留得住） |
| `zsched_history\zsched_YYYYMMDD_HHMMSS.txt` | 历史存档（§3-J） |

**每条日程里 v1.5 新增的两行**：`sndmode=0/1`（0 = 系统提示音，1 = 自选声音文件）、`sndfile=<路径>`
（路径里的 `\` 按应用格式写成 `\\`；老数据没有这两行 → 行为完全不变，旧版本读到会忽略）。

**数据目录**（`HKCU\Software\Jian\DataDir`）就是这四份东西的家：没设置时 = exe 所在目录（老行为不变），
改过之后桌面那份 exe 与工程那份 exe 会**共用**同一个目录（见 §3-N）。

---

## 3. 已完成事项

### A. 框架与三视图
- 纯 C + Win32，只依赖系统自带的 `gdiplus.dll / comctl32.dll / msftedit.dll`。
- 三视图：**清单**（纸卡片 + 倒计时胶囊）/ **表格**（8 列，点列头排序）/ **周表**（7 列 × 24 行自绘网格）。
- 周表：`Ctrl+滚轮` 或 `+/-` 五档缩放（60/80/100/130/170%）、当天列高亮、当前时刻红线、双击空白格新建、双击条目编辑；缩放时保持视口中心时刻不变。
- 纸质主题 `paper.c`：程序化纤维纹理（96×96 瓦片 + `CreatePatternBrush` 缓存 + `SetBrushOrgEx` 对齐屏幕）、柔和投影、书页折角。
- 全部布局由字体度量推导并按 DPI 等比缩放，100%/150%/200% 不裁字。

### B. 便笺式桌面窗口 `noteview.c`（1902 行）
- 单条 / 批量打开；两种外观可切换：**便笺纸**（无边框分层窗口 `WS_EX_LAYERED`，逐像素 alpha 算法线阴影）/ **系统窗口**。
- 8 种纸色、置顶开关；悬停圆点换色、图钉切置顶；**位置 / 尺寸 / 颜色 / 外观 / 置顶全部持久化**（`zsched_notes.txt`）。
- 交互：任意处拖拽移动、右下角缩放、双击进编辑、右键菜单。
- **拖动变形 bug 已修**：`WM_MOUSEMOVE` 里只改 left/top 会把窗口越拖越小，现在先取 `dragRc` 的 w/h 再重算四边。
- **内容自适应**：`note_metrics()`（noteview.c:148）以 240×180 为基准算 fit 因子
  → `n->fontScale = MulDiv(scale, fit, 100)`，便笺放大字体自动跟着变大（clamp 50..700，4% 取整防抖）。
- **正文始终绘制**：去掉了旧的 `drewBig` 门控（照片一出现就把正文挤掉的那个 bug）。
- **照片小的时候正文自动上移填空**：正文起点用照片真实最低边 `L->imgBottom`，不再用预留带高。

### C. 便笺墙 / 笔记本页 `stickies.c`（2446 行）
- 便笺在墙上**自由拖动位置**、**可固定（pinned）**、**可缩放大小**、可隐藏/显示（单张/全部），全部持久化到 `zsched_board.txt`。
- 流式自动排布（`IDM_BOARD_AUTO / AUTOALL`）。
- 内容分区为「文字优先」：标题 → 日期 → 倒计时框 → 正文（视觉区 ≤60%）→ 照片（≥40%）；`IMGM_FLOAT/INLINE` 时照片在正文上方，`IMGM_END` 时在正文之后。

### D. 富文本逐段字体字号 `richtext.c`（724 行）
- 自研 RTF 解析：`rich_parse()` → `RichRun` 列表，支持 `\fN`、`\fsN`（半磅）、`\b \i \ul`、`\'hh`、`\uNNNN`、`\par`。
- `rich_draw()` 逐 run 换字体绘制；HFONT 缓存键 = 字体名 + 半磅 + 粗/斜/下划线 + 缩放比（`rfont_get`）。
- **换行算法**：先整段测量，放不下就**二分**找最长可放前缀 —— 不能用 `GetTextExtentExPointW` 的 `lpnDx`（对 CJK 不可靠）。
- `rich_decode_name()` 解 ANSI 转义字体名（华文楷体 = `\'bb\'aa\'ce\'c4\'bf\'ac\'cc\'e5`）。
  注意 `\uc1`、`\uc0` 是控制字**不是** Unicode 转义，只有 `u` 后紧跟数字或负号才算。
- 编辑窗支持**选区级**改字体/字号/B/I/U + 「整篇应用」。
- **「字体只在编辑框显示」的 bug 已修**（三处）：①字体表跳过循环遇到 `\'` 要停下；②去掉 `drewBig` 门控；③桌面便笺接上 `fontScale`。

### E. 图片 `image.c`（669 行）+ `imgview.c`（466 行）
- 每条日程最多 **6 张**（`MAX_IMGS`），每张有**独立**千分比框 `imgB[MAX_IMGS][4] = {x,y,w,h}`（相对内容区），所以缩便笺不会串位。
- **独立移动 / 独立缩放**：命中 8 个方形手柄 + 内部区域（`img_handle_at` / `st_img_hit` / `note_img_hit`，反向遍历取最上层）；拖动模式 `imgMode 1=移动 2=缩放`。
- **版式**（参考 Word）：浮动 / 随文（文中）/ 文末（`IMGM_FLOAT/INLINE/END`）。
- **层次**：浮于文字上方 / 衬于文字下方（`IMGL_OVER/UNDER`）。
- 菜单：照片 40/55/75/100% 缩放、版式、层次、位置与大小复位（`IDM_BOARD_IMGRST`）。
- 原图查看器 `imgview.c`：100% 原始像素、缩放、适应窗口、拖拽平移、多图切换。
- 格式：GDI+ 优先，**WIC 回退**（WebP / HEIC / AVIF）；WebP 必须「读入内存 → 从流解码」。
- 全分辨率缓存 `FullEnt` / `img_get_full()` + `img_draw_cover_full()`（cover 裁剪绘制）。
- ⚠️ **GIF / WebP 动图目前只显示首帧**，没有逐帧动画（见 §5 待办）。

### F. 滚动（用户反复强调的部分，已完成）
- **独立一列细滚动条**：便笺墙与桌面便笺**都有**，只在 `maxScroll > 0` 时绘制 —— 满足「只要有东西显示不全就要呈现滚轮」。
  - 便笺墙：`r.right - 11‰ .. r.right - 2‰`（stickies.c:1211 起，轨道 + 滑块两段 `FillRect`）。
  - 桌面便笺：`cr.right + NSC(2) .. cr.right + NSC(11)`（noteview.c:743 起），在内容区之外，不压内容。
- **单点计算 `maxScroll`**：历史上 `st_note_layout()` 里有两处重复赋值，后一处把修好的值覆盖回 0 → 滚轮永远是 0。现在只在函数末尾算一次（stickies.c:833–845 / noteview.c:422–434）。
- 滚轮优先级：`st_img_wheel()`（仅按住 Ctrl 才缩放照片）→ `st_note_wheel()`（便笺内滚动）→ 墙体/视图兜底翻页。

### G. 提醒功能（`ui_main.c`）
- 每条日程可**任意组合**三种方式（位标志 `remind` = `REM_POPUP|REM_TOAST|REM_SOUND`）：
  1. **便笺纸样式桌面弹窗**：独立窗口类 `ZScheduleRemind`（`REMP_CLASS`，ui_main.c:961），纸色**跟该条日程的便笺颜色一致**，点击或 **30 秒**自动关（`SetTimer(..., 30000, ...)`）。
  2. **通知栏（Win11 右下角）**：`Shell_NotifyIconW` 的 `NIM_ADD/NIM_MODIFY` 气泡。
  3. **提示音**：`MessageBeep`。
- 各自可自定义：**提前分钟** `remLead`、**重复间隔分钟** `remRepeat`、**次数** `remTimes`。
- 驱动：主循环 `main_tick()` → `remind_tick()`（ui_main.c:1239），状态表 `static RemState g_rem[128]`。
- 编辑窗里 **y=722** 那一排就是提醒控件：`hRemPopup / hRemToast / hRemSound / hRemLead / hRemRepeat / hRemTimes`（定义见 ui_edit.c:660–674，`EW_H` = 812）。
- ⚠️ **提醒的运行状态是内存态**，重启程序后重新计数（见 §5 待办）。

### H. 开机自启动
- 文件菜单可勾选（`IDM_FILE_AUTORUN`）；注册表 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，**值名 `Jian`**（ASCII 最稳），数据为 exe 全路径。
- 实现：`app_autostart_get/set`（ui_main.c:1272 起，`kRunVal`）。

### I. 历史版本存档
- 每次写盘前自动把**上一次**的数据存一份到 `zsched_history\zsched_YYYYMMDD_HHMMSS.txt`，**≥120 秒**节流（`MODEL_HIST_GAP`），最多保留 **60** 份，超出删最旧。
- 文件菜单：**存档当前版本**（手动立即存，`Ctrl+Alt+S`）、**打开历史版本文件夹**、**恢复最近一个历史版本**（恢复前会先把当前版本也存一份，不会丢数据）。
- 实现：`model_archive` / `model_history_dir` / `model_history_count` / `model_history_latest` / `model_history_restore_latest`（model.c）。

### J. 图标 + 改名「笺」（最新一轮）
- `app.ico`（185,343 字节）：把用户给的 1080×1080 图（留在工程里的 `app_icon.png`）用系统自带 .NET + `System.Drawing` 高质量重采样出 **16 / 24 / 32 / 48 / 64 / 128 / 256** 七档，按 ICO 容器格式打成 **PNG 压缩多尺寸 ico**（Vista+ 原生支持）——**没有引入任何第三方依赖**。
- `zsched.rc` 加 `1 ICON "app.ico"`；`zsched.h` 加 `#define APP_TITLE L"笺"`、`#define APP_ICON_ID 1`。
- `ui_main.c`：窗口类大/小图标与托盘图标都用 `LoadImageW(hInst, MAKEINTRESOURCEW(APP_ICON_ID), ...)`；托盘 `szTip`、通知 `szInfoTitle`、关于对话框首行都走 `APP_TITLE`。
- 名称统一为「笺」：**窗口标题 / 任务栏 / 托盘提示 / 通知标题 / 关于**；输出文件名 `笺.exe`（`build.bat`、`Makefile`、`README.md` 三处都同步过了）。
- 桌面副本同名，旧的 `ZSchedule.exe`（工程目录 + 桌面）已删除。
- 验证：`ExtractAssociatedIcon` 能读出 32×32 图标；运行进程窗口标题实测为 `[日程便笺]` 与 `[笺]`。

### Y. 后台运行（最新一轮，v1.6）
- 需求：**不必占用任务栏，可以在后台查看**。
- 常驻托盘图标：`tray_add()`（`Shell_NotifyIconW(NIM_ADD)`，`uID=1`，`uCallbackMessage = WM_APP+1`）
  在 `main_create()` 里调用；退出时 `tray_remove()`（`NIM_DELETE`）。
  原来只有 `remind_toast()` 懒加载图标，现在**启动就有**，toast 走的是同一条 `NIM_MODIFY` 路径。
- `WM_APP+1` → `LOWORD(lp)`：左键单击 / 双击 = `set_background(!g_background)`；右键 = `tray_menu()`。
  托盘菜单：显示主窗口 / 后台运行（勾选态）/ 打开全部便笺 / 停止铃声（有声音时才亮）/ 退出。
- `set_background(on)`：`ShowWindow(SW_HIDE)`（隐藏后任务栏按钮自然消失）→ 首次进入时发一条 `NIF_INFO` 气泡；
  回来时 `SW_SHOW` + `SetForegroundWindow`。**便笺窗口、提醒弹窗、通知栏都是独立窗口，后台状态下照常工作**。
- 文件菜单「后台运行（不占任务栏）」（`IDM_FILE_BACK = 2043`），勾选态在 `WM_INITMENUPOPUP` 里刷新
  （为此把文件菜单句柄存进 `g_mFile`）。

### X. 表格视图关键词搜索（最新一轮，v1.6）
- 搜索框是一个普通 `EDIT`（`IDC_SEARCH = 1016`），**只在表格视图显示**：`set_view()` 里 ShowWindow + 重新 layout；
  `layout()` 在表格视图时于内容区顶部留 32 逻辑像素给搜索框（右对齐 320 宽），表格整体下移。
- 过滤：`g_search[128]` + `w_find_ci()`（不区分大小写的子串）+ `table_match()`（标题 / 正文 / 链接）。
  `table_refresh()` 里不命中的行**不建**，行号用独立计数器 `row`（原来的 `i` 直接当行号，过滤后必须改），
  收尾处 `g_shownCount = row`、`g_searchRows = 搜索 ? row : -1`。
- **`same_order()` 要认识过滤状态**：否则 `g_orderCount != g_shownCount` 会让主循环每秒都判定“顺序变了”而整表重建；
  现在过滤时改为“按命中规则逐个比对 id/顺序”。
- 状态栏 `update_status_default()` 在表格视图 + 过滤时追加「搜索「xxx」：N 条」。
- 键盘：`Ctrl+F` → `do_search_focus()`（切到表格 + 聚焦 + 全选）；`Esc` → 清空（`SetWindowTextW(hSearch, L"")` 会触发
  `EN_CHANGE` 再走一遍过滤）。搜索框的提示文字用 `EM_SETCUEBANNER`。

### W. 便笺容量上限移除 + 便笺墙翻页（最新一轮，v1.6）
- **容量**（`noteview.c`）：`NoteWnd g_notes[NOTE_MAX]` → 动态数组（`note_ensure()` 翻倍扩容，新槽清零），
  `note_alloc()` 满了就扩容；`NoteLook g_lookMem[]` 也改成动态（上限 4096 条，只是外观缓存）；
  `note_load_state()` 的 `slots < 24` 改成 `NOTE_RESTORE_MAX`（0 = 不限）。
  `do_note_all()` 的 `int ids[24]` 改成按 `model_count()` 分配（便笺墙那边 `stickies_set_items` 也不再截断）。
- **翻页**（`stickies.c`）：
  - `BoardPos.page`（1 起，0/1 都算第 1 页）；`Sticky.page / pageCount / pageFull / footerH / fPrev/fNext/fNew`。
  - `pos_find(id, 1)` 新建记录时 `page = g_stk->page` —— **在某一页新建的便笺就记在这一页**；
    `stickies_set_items()` 给每个日程先建一条记录（并顺手把布局数组扩容）。
  - `st_layout()` 只摆当前页（其它页 `SetRectEmpty`）；`st_build_z()` 只收当前页；
    `st_recalc()` 顺带算出 `pageCount = max(各便笺页码, 当前页, 存过的 pages)`、`contentH`（含页脚）与 `pageFull`。
  - **页码栏**画在页面底部居中（`st_draw_footer`）：`‹ 第 n / m 页 ›` + 新开一页按钮；
    `pageFull` 时按钮变朱红实心、文字变「本页已满 · 新开一页」；`st_footer_rects()` 同时给绘制与命中用。
  - 交互：`WM_LBUTTONDOWN` 先判页脚三个按钮（再判便笺）；`PgUp/PgDn`（清单视图、主循环快捷键里）；
    视图菜单「上一页 / 下一页 / 新开一页」；便笺右键「这张便笺在第 N 页 / 移到上一页 / 移到下一页」（`stickies_move_page`）。
  - 持久化：`page=`（每张便笺，>1 才写）、`viewpage=`（当前页）、`pages=`（总页数）；`pos_load` 三个都读。
  - `stickies_hidden_count()` 改成**只数当前页**；“这一页都收起来了”的提示也按当前页判断。
  - `stickies_set_sel()`：选到别的页的便笺时自动翻到那一页（表格/周表选中能联动）。
  - `st_hit_ex()` 不再用 4096 的栈上 `order[]`，改为复用 `st_build_z()` 的 `z[]`（动态、且天然带页过滤）。

### AF. v1.9.4 便笺墙复制/粘贴 + 关于页
- **需求**：① 便笺墙原来的「复制便笺」细化为「复制 / 粘贴」，以便把一张便笺复制到**新开的一页**；
  ②「帮助 → 关于」显示当前版本号、作者「祝筱笺」、项目主页 GitHub 链接。
- **复制/粘贴实现**：
  - `ui_main.c` 增加内部剪贴板 `static Schedule g_clip; static int g_clipValid;`
    （`Schedule` 是纯值结构，整份赋值即可；不碰系统剪贴板，退出即失效）。
  - `do_copy(id)`：快照 + `EnableWindow(g_btnPaste, TRUE)`；`do_paste()`：`tmp = g_clip; tmp.id = 0; model_add(&tmp)`
    → `model_save()` → 不在清单视图先 `set_view(ZS_VIEW_LIST)`（否则 `main_rebuild_views`）→
    `stickies_set_sel(新 id)` → `stickies_save_pos()`。
  - 新便笺的页归属：`stickies_set_items → pos_find(id, 1)` 在**当前页**（`g_stk->page`）建档，
    所以「新开一页 → 粘贴」落在新页。粘贴的标题保持原样（不带「（副本）」）。
  - 入口：工具栏 `IDC_BTN_COPY`（以前没接命令，死项）+ 新按钮 `IDC_BTN_PASTE`（`layout()`/`WM_GETMINMAXINFO`
    的 9 项数组扩到 10 项，视图分段起点 5→6）；编辑菜单保存 `g_mEdit`，`WM_INITMENUPOPUP` 按剪贴板状态启停；
    右键菜单同样两项；`Ctrl+C`/`Ctrl+V` 快捷键，`Ctrl+D` 保留为复制旧别名。
  - **搜索框例外**：`main_handle_shortcut` 里 `h == g_hSearch` 时 `Ctrl+C/Ctrl+V` 直接 `break`，交给 EDIT 自己处理。
- **顺带修好的页问题**（做跨页粘贴时暴露）：
  1. 换页/新开页时 `s->selId = 0`：以前旧页的选中项被 `main_rebuild_views` 恢复，一重建就把页跳回旧页，
     导致粘贴落错页；
  2. 新增 `stickies_save_pos()`，粘贴后立即把新便笺的 `page=` 落盘（否则重启后回到第 1 页）；
  3. `st_recalc` 初始页改用 `g_viewPage`（恢复上次看的页）；
     新增 `g_boardInit`：**第一次铺墙**时，墙信息里没有页记录的历史便笺默认第 1 页，而不是被记到“当前页”。
- **关于**：
  - `zsched.h` 新增 `APP_VERSION L"1.9.4"`、`APP_AUTHOR L"祝筱笺"`、`APP_URL L"https://github.com/BlessedLETTER/BLESSEDLETTER/tree/BlessedMemo"`。
  - `show_about()` 改为 `TaskDialogIndirect`：主标题 + 版本/作者 + 原功能/快捷键说明（补 Ctrl+C/Ctrl+V），
    页脚是 `TDF_ENABLE_HYPERLINKS` 的超链接；`about_td_cb` 处理 `TDN_HYPERLINK_CLICKED` → `ShellExecuteW`；
    另有「打开项目主页」按钮；`TaskDialogIndirect` 失败时退回带版本/作者/URL 的 `MessageBoxW`。
- **验证**（截图见 `_tools/shots/v194/`，测试数据 `_ttest4`）：
  - 选中 A → 工具栏「复制」→ 点「新开一页」（页 2/2，空页、选中已清）→「粘贴」：
    **页 2 出现 A 的副本并被选中**，`zsched_board.txt` 里 `viewpage=2 pages=2 id=4 … page=2`（`paste_page2.png`）。
  - 重启程序：仍停在**第 2/2 页**，页 2 只有那张粘贴的便笺（`restart_page2.png`）——页归属持久化正确。
  - 表格视图按 `Ctrl+V`：自动切回清单视图，新便笺出现在当前页并被选中（`table_paste.png`）。
  - 工具栏「复制 + 粘贴」端到端：新增一张（共 6 条），新便笺选中（`toolbar_paste.png`）。
  - 空剪贴板：工具栏「粘贴」、编辑菜单、右键菜单都置灰（`editmenu_empty.png` / `ctx_empty.png`）。
  - 搜索框里 `Ctrl+V` 粘贴的是系统剪贴板文字，**没有**新增日程（仍 5 条）。
  - 「关于」：`版本：v1.9.4`、`作者：祝筱笺`、蓝色超链接 + 「打开项目主页 / 关闭」按钮，Esc 关闭（`about.png`）。
  - 构建：`笺.exe` 743,863 字节，零 error / 零 warning，MD5 `3eb2c2bc7995995869ed1a6ee49e470c`。

### AE. v1.9.3 便笺滚到底 / 照片放大后正文消失（加急）
- **反馈**：桌面「CMC数竞初赛」便笺，滚轮翻到最底、或把图片放大，正文反而被挤掉消失。
- **根因**（四处叠加）：
  1. **正文被滚了两次**：布局 `noteview.c:638` 已经 `NL->textTop -= NL->scroll`，
     绘制 `noteview.c:1227` 又 `r3.top = vTop - NL.scroll`。照片上移 1×、正文上移 2×，
     滚到 `maxScroll` 时正文整段跑到裁剪框**上方** → 字消失。便笺墙 `stickies.c:1134` + `1472` 完全相同。
  2. **裁剪框跟着正文起点跑**：`IntersectClipRect` 的上边是“已滚动的正文起点”。照片把正文顶到便笺下方时，
     这个起点本身就在可视区外，滚轮永远滚不进来。
  3. **照片手动放大后完全不预留正文**：`imgSized` 时 `imgH = availBody`（不减去 `thUse`），
     `textTop` 落到便笺底边之外；`IMG_MIN_H` 分支还会把 `thUse` 压到 0。README 的“至少一行”其实没实现。
  4. 照片/日期不参与内容裁剪：滚动时照片会盖住固定的标题。
- **改动**：
  - 绘制正文：`r3.top = NL.textTop`（只减一次）；裁剪上边改用 `NL.viewTop`（布局在滚动前记下的内容视口顶），
    下边 `cr.bottom`；外层给日期/倒计时/UNDER 照片/正文再套一层同样的裁剪，标题留在裁剪外；
    OVER 照片与选中框单独用同一视口裁剪。便笺墙的 `L.bodyTop` 绘制同样只减一次，外层裁剪同口径。
  - **正文始终留一行**（有 `note`/`link` 就生效）：
    便笺 `reserve = imgSized ? keep : max(thUse, keep)`，其中 `keep = lhText+gap + 开头空行数×lhText`；
    墙卡片同理 `keep = textMin+gap + 开头空行数×lhSmall`；
    都夹到 `availBody`，`imgH = availBody - reserve - 文末版式的 gap`。
    开头空行也算：用户的「CMC数竞初赛」正文就是 `\n\n高等代数…`，只留一行会留下空行、字仍在下面，
    所以把开头连续空行一并算进预留（`note_lead_blank_lines` / `st_lead_blank_lines`）。
    `IMG_MIN_H` 下限只在 `availBody - minH >= reserve` 时生效；`slot` 不超过 `imgH`。
  - 拖动夹紧同步：`note_img_clamp` 用同样的 keep 高度；`st_img_min_text(L, sc)` 改成按“这张日程有没有正文”返回，不再看 `imgSized`。
  - 附件行、滚动条、悬停按钮保持固定，不受内容裁剪影响。
- **验证**（截图 `_tools/shots/v193/`）：
  - 测试便笺：620×620 物理、400×1200 竖图、10 行正文。顶部：日期/倒计时/照片 + 第一行正文
    （`note_scroll_top.png`）；滚轮往下，正文逐行进入视口，**滚到最底最后一行贴在便笺底边**
    （`note_scroll_bottom.png`）；再滚回顶部，照片与第一行恢复（`note_scroll_back.png`）。
  - 照片放大到 `imgb0=0,0,1000,300`（满宽）后：照片占满照片带，**底部仍留一行正文**；
    往下滚能看到其余正文（`note_moveclamp.png` 为拖动夹紧验证：照片停在正文最后一行上方）。
  - 回归：纯正文便笺（`note_text_only.png`）滚动正常；纯照片便笺（`note_photo_only.png`）照片仍占满整个正文区。
  - 便笺墙卡片：滚到底最后一行可见（`card_scroll_bottom.png`），上滚时内容被裁在视口内、不压标题。
  - 用户在用的真实便笺「CMC数竞初赛」（正文开头有两个空行、照片 1000‰）：顶部照片 + **实际的文字行可见**
    （`real_cmc_v193_top.png`）；滚轮往下文字仍在、不再消失（`real_cmc_v193_bottom.png`）；滚回顶部恢复（`real_cmc_v193_back.png`）。
  - 构建：`笺.exe` 741,371 字节，零 error / 零 warning，MD5 `fb78b62d65990d4c941bb4bed4d996ee`。

### AD. v1.9.2 照片「指针离开就消失」+ 点选编辑（加急）
- **反馈**：桌面独立便笺上的照片，鼠标指针一离开就整张消失；而且指针一放到照片上就出现手柄，像“自动进入编辑”。
  要求：修掉消失；**右键退出图片编辑模式**；不要指针一悬停就进编辑。
- **根因**：
  1. `noteview.c` 画「浮于文字上方」那一层照片的条件是 `if (drewBig && n->imgHotIdx >= 0)`。
     早先 `imgHotIdx` 从不清零（calloc 后是 0，看起来照片一直在），v1.9.1 为了指针离开时收起手柄，
     在 `WM_MOUSELEAVE` 里把它置成 -1 —— 于是**照片整张不画了**（先有“清悬停”，后有“按悬停画”，两者撞在一起）。
  2. 虚线框 + 四个角手柄是按**悬停**（`imgHotIdx`）画的，没有“选中”这个状态，指针扫过就出编辑框。
  3. 便笺墙 `stickies.c` 的 `st_draw_images` 同样把 `imgActive`（悬停/拖动）当成画手柄的条件。
- **改动**：
  - `noteview.c`：新拆分 `note_draw_images()`（只画照片 + 细边框，**无条件**）与 `note_draw_img_sel()`
    （只给 `imgSelIdx` 画虚线框 + 四角手柄，最后统一画在最上层）。
  - **选中与悬停分离**：`imgSelIdx` = 点选的照片（初始 -1）；`imgHotIdx` 只用于光标（移动 / 斜向缩放）。
    `WM_MOUSELEAVE` 只清 `imgHotIdx`，照片与选中框不受影响。
  - 进入/退出：左键按在照片上才设置 `imgSelIdx`；单击便笺其他位置、或 `WM_RBUTTONUP` → 清掉 `imgSelIdx` 再弹菜单；
    双击照旧 `PostMessage(IDM_EDIT_EDIT)` 打开编辑窗。
  - `stickies.c`：`st_draw_images` 去掉 `imgActive` 参数；手柄只在 `s->imgSelIdx >= 0 && s->selId == sc->id` 时画；
    `stickies_create` 初始化 `imgHotIdx = imgSelIdx = -1`；卡片右键、非照片左键同样清选中。
  - **顺带修「框被最小尺寸撑开」**：两个布局（`stickies.c` / `noteview.c`）末尾各有
    `if (框宽 < minW) 撑到 minW` / `if (框高 < minH) 撑到 minH`。照片按比例缩好之后又被这条钳制撑大，
    contain 绘制下就变成左右/上下的一圈无效空白，选中框也比照片大一圈。
    现在**框永远贴住照片**；最小尺寸只在“拖角缩放”里防缩成一条线（布局侧不再二次撑开）。
- **验证**（截图在 `_tools/shots/v192/`）：
  - 400×1200 竖图：悬停到照片上**不出现任何手柄**（`note_hover.png`）；指针移到便笺外，**照片仍在**（`note_leave.png`，本次主 bug）。
  - 单击照片：虚线框 + 四角手柄**贴住照片**（`note_select.png`）。
  - 拖右下角向内：`imgb0` `376,308,230,691` → `376,308,159,482`，左上不动、宽高同比例（照片 1:3 不变形）；
    拖本体：`378,259,159,482`（x/y 变、w/h 完全不变）。
  - 右键照片：手柄/虚线框消失、照片保留，便笺菜单正常弹出（`note_rclick.png`）；`Esc` 关菜单后照片仍在。
  - 双击照片：`ZScheduleEditWnd|编辑日程` 打开（`Esc` 取消后便笺正常）。
  - 便笺墙卡片：悬停无手柄（`card_hover.png`）；单击出手柄且**贴住照片**（`card_select.png`）；
    右键取消选中 + 卡片菜单（`card_rclick.png`）。
  - 诊断：墙上卡片布局实测 `cw=390 wf=400 slot=283 → w=94 h=283 minW=156`，最后被 minW 撑成 156×283
    （照片只有 94 宽，左右各 31px 空白）——删掉钳制后框 94×283，与照片严丝合缝。
  - 构建：`笺.exe` 739,835 字节，零 error / 零 warning，MD5 `52f3d895fe494b170aafd1c1e4568b8e`。

### AC. v1.9.1 便笺墙卡片的照片（紧急补齐）
- **背景**：v1.9 只改了独立便笺（`noteview.c`），清单视图的便笺墙（`stickies.c`）还是旧逻辑：
  cover 裁切 + 八个手柄 + Ctrl+滚轮缩放 → 用户反馈“便笺本界面这个问题还没修复”。
- **改动**（全部与 v1.9 对齐）：
  - `StkLayout` 增加 `imgAreaTop/imgAreaBottom`、`imgSized`；布局里**没有正文时 `thUse = 0`**，
    用户调过大小（`imgB[i][2] >= 60`）时照片区直接用满正文区。
  - 每张照片的框：`w = cw × 宽度千分比`，`h = w × 图高/图宽`；`h` 超过槽位就整体缩小 —— **框和照片永远同比例**。
    ⚠️ 关键教训：最初版本给 `h` 加了“最小高度”夹紧，**只抬高度不抬宽度** → 框的比例变了，
    `img_draw_fit` 只能在框里 letterbox（宽照片上下留白）——截图里一眼看出“框比照片大一圈”。
    现在**不再夹高度下限**（只钳宽度），比例恒等；最小尺寸只用在“拖角缩放”那一侧（48/36 逻辑像素，防缩成一条线）。
  - 浮动照片的纵向位置改按 `L->content` 换算（和 `img_box_store` 的存盘基准一致，拖完不会漂）。
  - 绘制 `img_draw_cover_full` → **`img_draw_fit`**；手柄从 3×3 的八个改成**四个角**（白底朱边小方块）。
  - 命中 `st_img_hit`：新增 `st_img_corner()`，**只认四个顶角**（热区 `IMG_HDL+4`，≥10px）；本体 hdl=0。
  - 缩放（`imgMode == 2`）：固定对角，两维同比例，上限到 `imgArea0`；最小尺寸用 DPI 单位（不随卡片字号缩放）。
  - **拖动夹紧 `st_img_clamp` 重写成“只挪位置、不改尺寸”**：以前它会用 `IMG_MIN_W/H` 改宽高，
    结果拖一下照片就变形（实测拖动后 `imgb0` 的 w/h 都变了）。`noteview.c` 的 `note_img_clamp` 同样重写。
  - 删除 `st_img_wheel()`（Ctrl+滚轮缩放照片），`WM_MOUSEWHEEL` 直接走卡片正文滚动。
- **验证**（截图 `_tools/shots/v191/`）：
  - 宽图（1200×300）/ 竖图（400×1200）在墙上卡片里都**整张可见**，框与照片同比例（截图 `wall_wide_contain.png`）；
  - 悬停出现**四角手柄**（`wall_corner_handles.png`）；
  - 拖右下角：`imgb0` 从 `0,522,1000,496` → `0,522,839,413`（左上不动、宽高同比例）；再往外拖被卡片宽度正确挡住；
  - Ctrl+滚轮后 `imgb0` 不变（不再缩放照片）；
  - 双击卡片照片 → 打开 `ZScheduleEditWnd`；
  - 独立便笺改完夹紧后仍正常：拖本体 `0,300,1000,400` → `0,628,1000,400`（尺寸不变，只移到新位置）。

### AB. v1.9 独立便笺的照片交互（用户反馈）
- **反馈**：① 照片缩放不便——四个顶角没有“两维同时缩放”，且容易和拖动位置混淆；
  ② 希望能“双击进编辑窗、单击选中拖动”；③ 照片的尺寸被便笺形状限制，扁便笺里竖图显示不全，下面还留无效空白。
- **重做后的规则**（`noteview.c`）：
  - **绘制**：`img_draw_fit()`（image.c，新函数）= 等比 contain：取 min(sx,sy) 缩放到框里、居中、不裁剪；
    框本身按“照片宽高比”生成（宽度取 `imgB[i][2]` 千分比，高度 = 宽 × 高/宽比），所以框里没有空白、照片永远完整。
  - **布局**：`imgSized`（`imgB[i][2] >= 60`）= 用户手动调过大小 → 照片区 **不** 再给正文预留高度
    （`imgH = availBody`），否则 `imgH = availBody - thUse`；**没有正文时 thUse = 0**（以前固定预留一行）。
    浮动照片的纵向位置改按 `NL->content` 换算（和拖动/缩放存盘用同一基准，避免位置漂移）。
  - **四角手柄**：`note_img_corner()` 只认四个顶角（`IMG_HDL_L|T` 等组合，热区 `NSC(n, IMG_HDL+4)` 且 ≥12px）；
    `note_draw_images()` 在悬停/操作过的照片上画虚线框 + 四个白底朱边小方块；鼠标离开便笺时收起（`WM_MOUSELEAVE` 清 imgHotIdx）。
  - **缩放**（`mode == 4`）：固定对角、按光标到固定角的距离算新宽；**两维同比例**（以相对变化大的那一维为准），
    最小 `IMG_MIN_W/H`，最大到 `imgArea0`（照片可用区）+ 内容区；存 `imgB[kk][2]`（宽度千分比）与 `[0]/[1]`（左上角）。
  - **交互**：单击照片本体 → `mode 3`（只改 x/y，尺寸不变，`imgMoved` 区分“点了没动”）；双击 → `WM_LBUTTONDBLCLK` 直接
    `PostMessage(IDM_EDIT_EDIT, hi=1, id)` 打开编辑窗；四角优先于便笺右下角手柄（否则照片贴边时抓不到角）。
  - **光标**：角上 `IDC_SIZENWSE / IDC_SIZENESW`（按对角方向），本体 `IDC_SIZEALL`。
- **验证**（截图在 `_tools/shots/v19/`）：宽图/竖图 × 扁便笺/窄高便笺四种组合都“整张可见、无裁切、框内无空白”；
  拖右下角（`imgb0` 464,795 → 387,635，x/y 不变、宽高同比例）✓；拖本体（215,193 → 123,155，w/h 不变）✓；
  双击照片弹出 `ZScheduleEditWnd` ✓；便笺自身右下角手柄仍可缩放（1028×488 → 1090×530）✓；
  光标实测：角上 SIZENWSE、照片中间 SIZEALL、空白处 SIZEALL ✓。

### AA. v1.8 迁移功能 + 历史目录复制 bug
- **需求**：加入“迁移功能”，在桌面生成一份现有便笺数据的拷贝文件夹，便于数据迁移。
- **实现**（`ui_main.c` `do_data_migrate_desktop()` / `write_migrate_readme()`）：
  1. `SHGetFolderPathW(CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE)` 取**真实桌面**（含 OneDrive 重定向）；
  2. 文件夹名 `笺数据迁移_YYYYMMDD_HHMMSS`；同名就依次试 `_2`、`_3`…（同一秒连点也不会互相覆盖）；
  3. **先 `model_save(model_path())` + `note_save_state()`** 把内存态落盘，再 `app_data_copy_to(dst)`
     （主数据 + 便笺状态 + 便笺墙 + 历史版本）；
  4. 写一份 `迁移说明.txt`（UTF-8 + BOM，说明里面有哪些文件、怎么在新电脑上用）；
  5. `ShellExecuteW(open)` 打开这个文件夹，状态栏提示完整路径。
- 菜单：文件 → 数据保存位置 →「迁移：在桌面生成数据拷贝(&T)」（`IDM_FILE_MIGRATE = 2045`），
  放在「备份当前数据到文件夹…」上面。
- **顺带发现的真 bug**：`app_data_copy_to()` 复制 `zsched_history\*.txt` 时，
  循环里 `path_join(s, ..., srcHist, fd.cFileName)` 用的是**数据目录**（`srcHist` = `…\_ttest\`），
  漏了 `zsched_history\` 这一层 → `file_exists` 永远为假 → **历史目录从来都是空的**。
  也就是说 v1.3 以来的「备份当前数据」「切换数据目录时的迁移」都没带上历史版本（静默失败，没人报错）。
  修法：历史目录单独用 `histDir` 变量，pattern 与源路径都基于它。探针（`_tools/copyprobe`）复现 + 修复后复核：
  拷贝里 3 个历史快照全在。
- 附带：`_tools/pclose.ps1`（按标题关窗口，测试收尾用；**必须纯 ASCII**——PowerShell 5.1 按 ANSI 读无 BOM 的 .ps1，
  中文注释会把后面的 here-string 解析搞坏）。

### Z. v1.7 修正：课表接力改「下课后 5 秒」+ 提示音停止小窗
- **课表接力时间点**（用户报：“下课后虽然自动关闭，但不会在下课后立即把整个窗口换成下一节课”，要求改到“原课程下课后 5 秒”）：
  - 旧逻辑：`tt_tick` 里一到 `now >= en` 就 **立刻关窗**，然后只在“现在正好有课在上”时开下一节 →
    有课间空档时窗口先消失，要等到下一节开始才重新出现。
  - 新逻辑：`TtState` 增加 `time_t handoverAt`；`TT_HANDOVER_SEC = 5`。
    ① 主循环只负责：该开就开；`!in && inSession` 时**不关窗**，只记 `handoverAt = en + 5`；
    ② 第二个循环（到期交接）：`now >= handoverAt` → 记位置（`g_ttSlot`）→ `note_close_one` →
       `sched_class_now()`（连堂时就是正在上的那节）优先，否则 `sched_class_next(now - 5)` 取今天最近的一节 → `tt_open_class`。
       若下一节**还没到上课时间**，把它的 `inSession` 清 0 —— 这样它开始那一刻只会“认领”已开的窗，不会重复开。
    ③ `tt_handover_pending(now)`：交接待办的那 5 秒里，别的课不许提前抢开（否则连堂时会出现两个窗口叠在同一位置）。
  - 今天之后没有课了 → 5 秒后只关窗，不开新窗。
- **只勾提示音时的「停止铃声」小窗**（用户报：“仅勾选提示音，无手动关闭按钮，应添加”）：
  - 新窗口类 `ZScheduleRingWnd`（`ring_proc/ring_stop_show/ring_close_quiet`，330×124 逻辑像素）：
    便笺纸底（用该日程的纸色）+ `铃声提醒 · 这是铃声` + 日程标题 + 真按钮 **「停止铃声」** +
    `点下面的按钮停掉铃声（放完会自动关闭）`。点按钮或点窗口 → `snd_stop()` + 关窗。
  - **只在“有自选铃声、且没有勾便笺弹窗”时出现**（勾了弹窗就点弹窗停，不需要小窗）；
    带弹窗的提醒来时会先把小窗收掉（`ring_close_quiet`，先摘句柄再销毁，避免把刚响起的新铃声停掉）。
  - **播完自动关**：`sound.c` 现在记时长（MF 的 `MF_PD_DURATION`），`snd_is_active()` 在
    “已过时长 + 700ms”后返回 0（MCI 路径改用 `status zsring mode`）；小窗 400ms 定时器据此自行关闭。
    顺带让“停止铃声”菜单项的可用状态也变准了。

### V. 铃声内置播放 mp3 / flac（v1.6）
- 需求：铃声要**支持 mp3、flac 播放**（v1.5 是 ShellExecute 交给外部播放器，会弹出播放器）。
- 先用探针 `_tools/audioprobe.c` 摸了三种通道在这台 Windows 11 上的能力：
  | 格式 | MCI | DirectShow | Media Foundation |
  | --- | --- | --- | --- |
  | wav | ✓ | ✓ | ✓ |
  | mp3 | ✓ | ✓ | ✓ |
  | flac | ✗（err 263） | ✗（0x80040216 / VFW_E_UNSUPPORTED_STREAM） | **✓**（native subtype 0xF1AC，解出 PCM） |
  → 结论：**用 Media Foundation**，wav/mp3/flac 一条路全包（Win10+ 自带 FLAC 解码器）。
- `sound.c`（195 行）：
  - `mf_play()`：`MFStartup` → `MFCreateSourceResolver` + `CreateObjectFromURL(MF_RESOLUTION_MEDIASOURCE)`
    → `CreatePresentationDescriptor` + `GetStreamDescriptorByIndex(0)` → 建拓扑（`MF_TOPOLOGY_SOURCESTREAM_NODE`
    设 `MF_TOPONODE_SOURCE/PRESENTATION_DESCRIPTOR/STREAM_DESCRIPTOR`，输出节点用 `MFCreateAudioRendererActivate`）
    → `MFCreateMediaSession` + `SetTopology` + `Start`。**没做事件回调**（铃声短，不需要进度/错误处理）。
  - `mci_play()`：`open "文件" alias zsring` + `play`（老系统兜底，wav/mp3）。
  - `snd_play_file()` 返回实际通道（1=MF 2=MCI 3=系统程序 0=失败）：MF 失败 → MCI → `ShellExecute`。
  - `snd_stop()`（停 MF 会话 + MCI close）、`snd_is_active()`、`snd_shutdown()`（退出时 `MFShutdown`，main.c 调用）。
- 接入：`remind_fire()` 里**先收旧弹窗、再开新弹窗、最后播新铃声**（顺序很重要：弹窗销毁时会 `snd_stop()`）；
  `remp_proc` 的 `WM_DESTROY`（= 点击关闭 / 30 秒自动关）`snd_stop()`。
  编辑窗「铃声…」菜单：`试听（在本程序里播放）` 4303 / `停止铃声` 4306 / `用默认播放器打开` 4307。
- 编译链接要加 `-lwinmm -lmfplat -lmf -lmfuuid`（build.bat / Makefile 都已加）。

### U. CSV 课表导入（v1.5）
- 需求：导入 CSV 课表文件 → **识别格式内容**（**正则匹配**保证精度）→ 逐行**生成便笺形式**；
  **只识别便笺支持的字段（名称、时间），其余信息统一进正文框**。
- **新模块 `csvimport.c`（2141 行，唯一对外入口 `csv_import_show(HWND)`）**，里面分四层：
  1. **极简正则引擎**（本机工具链没有 `<regex.h>`，`find` 过确认）：
     - 语法子集：字面量、`.`、`[a-z]` `[^…]`（含 `\d \w \s` 类内转义）、`\d \D \w \W \s \S`、`* + ? {m} {m,} {m,n}`（贪婪）、
       `( )` 捕获（最多 9 组）、`(?: )` 非捕获、`|` 分支、`^ $` 锚点、可忽略大小写。
     - **编译**：递归下降 → 结点树（`ZN_LIT/ANY/CLASS/ALT/GROUP/REP/BOL/EOL`，固定 640 结点池，无 malloc）。
     - **匹配**：`m_chain(链, 续接)` / `m_cont(续接)` / `m_rep(...)` 的 **CPS（续接）实现** —— 把“后面还要匹配什么”显式传下去，
       所以组、分支、量词的失败都会正确回退（`(a|ab)c` 能命中 `abc`；量词 `[0-9]+5` 能吐字符给后面的 `5`）。
       组的终点用一个专门的“组结束”续接帧（kind=2）记录：内层链走完走到那一帧时就是组的结尾。
     - 首次用到时把 11 条模式编译一次（`re_setup()`），失败则整体禁用并提示。
  2. **CSV 读取**：编码自动识别（UTF-8 ±BOM / **UTF-16LE**（有无 BOM）/ **GBK(936)** → 退回 CP_ACP）；
     分隔符在 `,` `;` `Tab` `|` 里按“前 5 条记录谁多”自动定；**整缓冲按状态机切记录**（引号里的换行属于同一格，
     这正是 Excel 导出多行单元格的写法）；字段再按引号/`""` 转义切；每个字段做**全角→半角**归一 + 去空白。
  3. **识别（两种格式）**：
     - 表格型：前 8 行里关键词命中最多的一行当表头（≥2 个关键词），据此定「名称列 / 时间列 / 星期列 / 日期列 / 序号列」；
       没认到就按内容推断（名称列按“名字长度”加权——课名比教师名长；时间列按命中时间模式的行数）。
     - 网格型：第一行 ≥3 个单元格像星期、且第一列 ≥2 行像节次 → 判定为「行 = 节次、列 = 星期」，
       格子内按**空行**分段（一段 = 一节课），段内首行再用 `/ ／ | ｜ 、` 切出课名，其余进正文。
     - 时间取用优先级：**具体时刻 > 节次（按内置作息表推）> 默认 08:00（仅有星期时）**；
       有星期 → `REP_WEEKLY` + 星期掩码（起始日取“从今天起第一个星期几”）；只有日期 → 一次性；
       节次表见 `g_periodStart[12]`（第 1 节 08:00、第 2 节 08:55、第 3 节 10:00 …每节 45 分钟）。
     - **正文**：除名称外的所有列按「列名：值」逐行写进 `note`；时间类列只在“残值里还有有意义的内容”时才写
       （残值 = 把星期/时刻/节次/日期匹配段抹掉后剩下的、去掉标点与“周/节/第”等词后仍然非空的部分，
       例如 `周次=1-16周(单)` 会原样进正文，而 `周一 08:00-09:40` 不会重复写）；序号列（纯数字）直接丢。
  4. **确认框 + 落盘**：自绘模态窗 `ZScheduleCsvImportWnd`（类名）显示「格式 / 表头行 / 分隔符 / 名称列 / 时间列 /
     其余 N 列写入正文」+ 逐条预览（`1. 高等数学 周一 8:00-9:40 · 正文 3 行`），两个出口：
     「导入并打开便笺」（切清单视图 + `note_open_batch`，一次最多 16 张）/「仅导入到清单」/「取消」（Esc 也行）。
     导入的每条 `timetable=1` + `cdunit=CDU_MIN` + 调色板纸色轮转；**同名 + 同星期 + 同时间**视为重复直接跳过。
- 入口：主菜单「文件 → 导入 CSV 课表…(&I)」（`IDM_FILE_CSV = 2042`），紧接着「重新载入」那一行。
- **验证**（见 §7）：离线探针 6 组样例（表头型 / 星期+节次 / 网格多行单元格 / GBK+全角 / 无表头 / 带日期）+ 真机集成（导入对话框 → 落盘 → 便笺）。

### T. 提醒铃声：系统提示音 / 自选声音文件（最新一轮，v1.5）
- 需求：选提示音时可以**自由选择“系统铃声 / 自选文件”**，原理是**自动打开所选声音文件**，并且**窗口提示“这是铃声”**。
- **模型**：`Schedule.sndMode`（`SND_SYS=0` / `SND_FILE=1`）+ `Schedule.sndFile[MAX_PATH]`（zsched.h）；
  存取 `sndmode=` / `sndfile=`（路径走 `esc_append/unesc_into`，`\` 存成 `\\`）；
  `model_normalize()`：`sndMode` 只认 0/1，**自选模式但路径为空 → 退回系统提示音**（老数据两行都没有 = 系统提示音，行为不变）。
- **入口**（ui_edit.c）：「提醒」那一行的数字框后面放不下新按钮，于是把「铃声…」放在**「课表」那一行**的右半边
  （`IDE_REM_RING=3066`，x = CTLX+470，宽 92），右边跟一个只读 `STATIC`（`IDE_REM_RING_INFO=3067`）显示当前来源：
  `铃声：系统提示音` / `铃声：ring.mp3`（`snd_desc()` → `file_name_of()` 只显示文件名）。
  按钮 `TrackPopupMenu`：`系统提示音（Windows 默认）` / `自选声音文件`（勾选态）/ `选择声音文件…` / `试听（打开所选文件）` /
  `清除自选文件` / 灰字提示「提醒时自动打开所选声音文件」。**选完任一项会自动把「提示音」勾上**（不然选了也不会响）。
  文件对话框过滤 wav/mp3/wma/m4a/aac/ogg/flac/mid（`OFN_FILEMUSTEXIST`）。
- **触发**（ui_main.c）：`remind_fire()` 里 `REM_SOUND` 分支——自选且文件存在 → `snd_open_file()`；
  否则（系统提示音 / 文件没了）→ `MessageBeep(MB_ICONEXCLAMATION)`。
  `snd_open_file()` = `ShellExecuteW(g_hMain, L"open", path, NULL, NULL, SW_SHOWNORMAL)`，
  **不做音频解码**，交给系统默认程序，所以 mp3/m4a/flac 都能响。
- **弹窗提示**：`remp_paint()` 在时间行下面多画一行 `铃声：<文件名>（这是铃声）`（朱印红 RGB(150,62,54)），
  `remind_show_popup()` 相应地**把窗口加高 24 逻辑像素**（只在“提示音 + 自选文件”时）。
- 另加控件 id `IDE_REM_RING 3066` / `IDE_REM_RING_INFO 3067`；`snd_open_file` / `snd_desc` 在 zsched.h 里对外声明（便笺/其它窗口将来也能用）。

### S. 新增：课表便笺的「下一节」名称行（上一轮，v1.4）
- 需求：课表便笺的倒计时下方再显示**下一节课的名称**，点它能跳到那节课的编辑页；名称框与倒计时不能被吞掉；
  显示大小随弹窗自适应、不挤占其他内容；并且是一个与倒计时显示并列的**独立开关**。
- **模型层**：`Schedule.ttNext`（1 = 显示／0 = 不显示，旧数据按 1 算，`model_normalize` 钳位）；
  `sched_class_next_any(after)` 找“下一节课”（今天晚些的优先，今天没有就用各条日程的 `nextOcc`，明天/下周也算）；
  `tt_when_text()` 把时间写成“22:51 / 明天 08:00 / 周三 10:00”。
- **布局**（noteview.c `note_layout`）：`ttNextId/ttNextStart/ttName` 进 `NoteLayout`，名称行占箱内最下面一行；
  空间不够时 **先降倒计时字号（`boxNum`）、再让掉日期行**，保证“名称行 + 倒计时”都不被吞；正文按原优先级拿剩余空间。
- **绘制**：`note_draw_tt_next()` —— 先只按“课名”选字号（小号→正文→最小），放得下才把“什么时候”也拼上；
  全长都放不下才退到最小号 + 省略号。悬停时变蓝并加下划线。
- **交互**：`note_tt_hit()` 用同一套布局矩形命中；`WM_LBUTTONDOWN` 命中即进入 `mode = 5`（不触发拖动），
  `WM_LBUTTONUP` 时仍在同一行才 `PostMessage(main, WM_COMMAND(IDM_EDIT_EDIT, hi=1), nextId)` 打开那节课的编辑窗；
  `WM_MOUSEMOVE` 维护 `ttHot`（悬停），`WM_SETCURSOR` 按 `ttHot` 给手型光标。
- **开关**：编辑窗「倒计时」那一行新增 `IDE_TTNEXT`（只有勾了“课表”才可点，`edit_sync_kind` 联动）；
  便笺右键 `IDM_NOTE_TTNEXT`（勾选态，课表项才亮）；便笺窗口也响应同名 `WM_COMMAND`（方便脚本/其它窗口驱动）。
- **顺带修**：`main_tick()` 以前只刷新面板与三个视图，**没有重绘便笺窗口** → 便笺上的倒计时是一张“死图”；
  现在每秒 `note_refresh_all()`（秒级倒计时实测两张相隔 6 秒的截图不同）。

### R. 修复：收起残影 / 课表便笺倒计时 / 提醒次数不生效（最新一轮）
- **① 收起便笺留残影**（便笺多的时候最明显）：绘制循环是
  `for (i = 0; i < s->count; i++) { idx = s->z[i]; ... }`，而 `st_build_z()` 只填了 `n` 项（收起的便笺不进 z[]）。
  `n < count` 时，循环多走的几趟读到的是**上一轮遗留的 z 下标**，于是把已收起的便笺按旧矩形又画一遍 → 残影。
  修：`Sticky.zCount` 记录有效项数；循环走 `zCount`；画之前再 `if (st_hidden(sc->id)) continue;`；
  `st_rect()` 对收起的便笺直接返回空矩形（兜底，防止别的调用点再拿到旧矩形）。
- **② 课表便笺不显示倒计时**：`sched_class_window()` 只在“今天有这节课”时返回 1，之前的分支是
  「距上课 / 距下课 / **本节已下课**（无倒计时）」+「课表 · 今天没这节（无倒计时）」。
  修（noteview.c 与 stickies.c 两处一致）：今天这节已上完 → `「下节课 」+ cd_text(s->nextOcc)`；
  今天没这节 → `「还有 」+ cd_text(s->nextOcc)`；只有 `nextOcc <= 0` 才显示「课表 · 暂无后续」。
  「下节 xxx hh:mm」那一行只在**正在上课**时画（否则会和主行重复）。
- **③ 提醒的提前/间隔/次数改完不生效**：`ui_edit.c` 的 `edit_store()` 里用 `ctl_text(h, buf)` “读”三个数字框，
  但 `ctl_text()` 是**写控件**的（`SetWindowTextW`）→ 等于先拿未初始化/复用的 `nb` 覆盖控件，再 `wcstol(nb)` 解析它，
  用户的输入每次都被丢弃（症状就是“改了没反应”）。修：改用 `ctl_int(h, defv, lo, hi)`。
  另外 `RemState` 增加 `remind/lead/rep/times` 四个字段，`remind_tick()` 里只要这些和 `nextOcc` 有一项变了就
  `rs->next = fire; rs->fired = 0;` **重新计时**——否则次数用完后 `rs->next` 被推到 100 年后，改设置也永远不会再提醒。

### Q. 修复：便笺「收起」后摆不回来 + 误报「便笺都收起来了」（最新一轮）
- 现象（用户报）：清单界面收起便笺后无法放回；即使页面上还有便笺也提示「便笺都收起来了」（例：桌面那张「元旦节」）。
- **根因 1（命令没接）**：`IDM_BOARD_HIDE / SHOWALL / HIDEALL` 只写在 `show_context_menu()` 的局部 `switch(cmd)` 里，
  `main_proc` 的 `WM_COMMAND` 里没有对应 case → **主菜单（视图菜单）里点这几项完全没反应**（右键菜单能用）。
  修：在 `main_proc` 的 `WM_COMMAND` 补上三条，并在视图菜单加「收起选中的便笺(&H)」（`IDM_BOARD_HIDE`）。
- **根因 2（状态不刷新）**：视图菜单的项在 `WM_CREATE` 里按当时的 `stickies_hidden_count()`（此时便笺墙状态还没载入，恒为 0）算一次，
  之后再没刷新过 → 「显示所有便笺」永远是灰的；而负责刷新的 `WM_INITMENUPOPUP` 分支把菜单句柄写成了 `(HMENU)lp`，
  **实际在 `wParam`**（`lp` 低位是弹出菜单在菜单栏里的下标、高位是是否窗口菜单）→ 分支从来没进过，连「当前数据位置」标签也一直没刷新。
  修：改成 `(HMENU)wp`；打开视图菜单时实时 `EnableMenuItem` + `ModifyMenuW`（「把便笺全部收起来」⇄「显示全部便笺」）。
  教训：调试时先写一行日志（`lp=%p`）比盯截图快得多——一眼就看到 `lp=0000000000000003` 根本不是句柄。
- **根因 3（判定错）**：`stickies_hidden_count()` 数的是 `g_pos[]` 里所有 `hidden` 项，**包含已删除日程留下的残留记录**；
  画布上的提示判定 `stickies_hidden_count() >= s->count` 因此会在「还有便笺可见」时成立（例：1 张可见 + 1 条残留 = 1 >= 1）→ 误报；
  同时 `IDM_BOARD_HIDEALL` 的 `alloff = hidden_count() > 0` 也会被带偏。
  修：只数「当前这一页里、`model_by_id()` 仍存在」的便笺（没有便笺墙窗口时退化为「只数日程还在的」），
  并在 `pos_save()` 里把日程已不存在的 `g_pos` 记录就地清掉（`model_count() > 0` 时才清理，避免切数据目录的空档误清）。
- 用户现场数据（`zsched_board.txt`）正是典型受害样本：`id=2`（日程早已删除）`+ id=6/7` 全 `hid=1`。

### O. 正文逐段文字颜色（最新一轮）
- 需求：独立弹窗与非独立弹窗（便笺墙）的正文都要能**逐段选色**，参考 Word。
- **编辑窗**（ui_edit.c）：正文行新增 7 枚自绘色块（`IDE_TXTCOLOR_BASE + i`，第 0 枚＝自动色，画一道白斜杠表示）+「颜色…」按钮（`ChooseColorW`）。
  点击色块 → `fmt_apply_color()` → `rt_apply(..., colorSet, color, toAll=0)`，**只作用于选区**（无选区时是插入点颜色）；
  `fmt_sync_from_caret()` 会回读当前字色（`rt_get_format` 多返回 color/autoColor），对应色块自动加高亮框。
- **RTF 引擎**（richtext.c）：`rt_apply` 用 `CFM_COLOR`（+`CFE_AUTOCOLOR` 表示“自动/默认”）下发；解析侧新增
  ① `{\colortbl;\red..\green..\blue..;…}` → `RichDoc.colors[]`（下标 0 = 自动）
  ② 正文里的 `\cfN` → `RState.color` → `RichRun.color`（合并判断 `RD_SAME` 也带上颜色）
  ③ `rich_draw()` 每段 `SetTextColor(doc->colors[r->color])`，0 就用便笺默认字色。
- 便笺墙与桌面便笺共用 `rich_parse`/`rich_draw`，所以**两边都逐段上色**，不需要各自改绘制代码。

### P. 书签形态（最新一轮）
- 新增第三种外观 `NOTE_STYLE_BOOKMARK = 2`（zsched.h）：还是一张纸，但形状是贴在屏幕边上的**长条书签**。
- **几何**（noteview.c）：`bm_monitor()`（`MonitorFromRect` 取所在显示器）、`bm_nearest_edge()`（离哪条边最近）、
  `bm_rect_for()`（厚度 `NOTE_BM_THICK=34`、长度 `NOTE_BM_LONG=240` 逻辑像素，贴边放、长边沿边、位置跟着当前点并夹在显示器内）。
- **绘制**：`note_draw_bookmark()` —— 纸色底（`paper_fill` 带纤维纹理）+ 贴边那侧一道深色台 + 圆角外框 + 一端一个“挂绳孔”+ 标题；
  竖吸（左/右）时用带 `lfEscapement` 的字体（左 900 / 右 2700）把标题竖排，横吸（上/下）时正常横排，标题过长自动截断。
  分层渲染对书签用更小的投影深度（5 而非 12）并把 `UpdateLayeredWindow` 的落点对齐到纸面，这样它是**贴着屏幕边**的。
- **交互**：`WM_LBUTTONDOWN` 记 `dragged=0`；`WM_MOUSEMOVE` 里位移超过 4 逻辑像素就置 `dragged=1`；`WM_LBUTTONUP` 时
  拖动过 → `bm_nearest_edge` + `bm_rect_for` 重新吸边；没拖动（即单击）→ `note_set_bookmark(id, 0)` 还原。
  书签上不响应悬停按钮与缩放手柄（`note_hit_btn` / `note_hit_grip` 直接返回空）。
- **开关与还原**：`note_set_bookmark(id, on)`；开启时把当前矩形与外观记进 `restoreRc`/`restoreStyle`，还原时用 `note_reopen_layered()`
  （销毁窗口再按目标外观/位置重开，跟 `note_toggle_style` 同一套路，因为分层与非分层窗口样式不同）。
- **入口**：便笺右键「书签形态」（勾选态）、编辑窗「便笺…」菜单、便笺窗口也支持 `WM_COMMAND IDM_NOTE_BOOKMARK`（方便其它窗口/脚本驱动）。
- **持久化**：`zsched_notes.txt` 里书签多写 `edge=`、`rstyle=`、`rx/ry/rw/rh`（还原位置），载入时按这些恢复。

### N. 数据保存位置：统一目录 + 自定义路径 + 导入旧数据（最新一轮）
- 背景：以前四份东西都跟着 **exe 所在目录**走（`zsched_data.txt` / `zsched_notes.txt` / `zsched_board.txt` / `zsched_history\`），
  exe 一挪位置或换版本，数据就“看不见”——用户要求统一保存路径 + 自定义路径 + 能读旧版本便笺数据。
- **统一目录**：四份东西全部由“数据目录”派生（notes/board/history 本来就取自 `model_path()` 的目录，所以只要定下主数据路径就统一了）。
  目录存在注册表 `HKCU\Software\Jian` 的 `DataDir`（值名 ASCII，和自启动的 `Run\Jian` 一致，互不冲突）；没设置 = exe 同目录（老行为不变）。
- **model.c 新增**：`app_data_init()` / `app_data_dir()` / `app_data_dir_set(dir, migrate)` / `app_data_copy_to(dst)` /
  `app_data_find(AppDataCand*, max)` / `app_data_reload()` / `app_data_import(cand)`，加几个内部小工具（exe 目录、路径拼接、目录/文件存在、复制、数 token）。
  - `app_data_find()` 扫这些位置（去重、跳过没有 `zsched_data.txt` 的）：**当前数据目录 / exe 目录 / 桌面 / 文档 / 下载 / OneDrive 桌面·文档 / %APPDATA%\Jian / %LOCALAPPDATA%\Jian**，
    每份还给“日程条数（数 `[item]`）· 便笺条数（数 `id=`）· 最后修改时间”，按修改时间新→旧排序。
  - `app_data_copy_to()` 复制主数据 + 便笺状态 + 便笺墙 + 历史目录里的所有 `*.txt`（目标已存在的历史文件不覆盖）。
  - `app_data_import()` = **先 `model_archive()` 把当前数据存档，再切过去**；只换路径，**不复制也不删除**任何文件（两边都留着，这是“手动迁移”的安全解法）。
  - `app_data_reload()` 里**先 `model_free(); model_init();` 再载入**（第一版没清空，切目录时新旧数据会叠加成 10 条 —— 已修）。
- **main.c**：`resolve_data_path()` 不再自己算 exe 目录，改成「命令行参数 > `app_data_dir()`」；启动时先 `app_data_init()`。
- **热切换**：`data_switch()`（ui_main.c）= `note_close_all()`（旧目录的便笺状态先落盘）→ `app_data_dir_set()`（必要时先复制）→
  `note_path_forget()` + `stickies_path_forget()`（清掉缓存的状态文件路径）→ `app_data_reload()` → `note_load_state()` → 重建视图。不用重启。
- **菜单**：文件 → 「数据保存位置」子菜单 = 当前路径（只读标签，`WM_INITMENUPOPUP` 时刷新）+ 选择数据文件夹… + 迁移 / 导入已有数据… + 备份当前数据到文件夹… + 打开数据文件夹。
  选文件夹用 `SHBrowseForFolderW`（`BIF_NEWDIALOGSTYLE | BIF_EDITBOX`，回调里 `BFFM_SETSELECTIONW` 定位到当前目录）。
- **导入对话框**：自绘模态窗口 `ZSchedulePickDataWnd`（列表框 + 取消 / 改用这一份，双击即选，Esc 取消，居中且夹进工作区），
  每行 = 「N 条日程 · 便笺 M 条 · 最后修改 … 路径」。

### K. 课表模式（最新一轮）
- 数据字段 `Schedule.timetable`（`timetable=1` 落盘）；只有**普通日程**（`kind == KIND_TASK`、启用、未完成）能当课表，`model_normalize()` 会清掉其它情况。
- 时间窗：`sched_class_window()`（model.c）取「今天的这一次发生」→ `sched_occurrence_on(s, today_ord())` 拿当天的开始时刻，结束用 `sched_occurrence_end()`；**没填结束时间按 `TT_CLASS_MIN = 45` 分钟**算。
- 还在上课的是哪节：`sched_class_now()`（重叠时取最近开始的）；下一节：`sched_class_next(after, …)`。
- **自动接力**：`tt_tick()`（ui_main.c，由 `main_tick()` 每秒驱动，状态表 `g_tt[64]`）——
  ① 进入上课时间且便笺没开 → `tt_open_class()` 打开（沿用上一节的位置 `g_ttSlot`）；
  ② 下课且它的便笺开着 → 记下位置 `note_get_geom()` → `note_close_one()` → 立刻用 `note_open_at()` 把**现在正在上的下一节**开在同一位置；
  ③ 用户手动关掉的课表便笺不会被强行再开（下课那一刻如果便笺是关的，就只更新状态、不做接力）。
- **外观记忆**：`g_lookMem[]`（noteview.c）记住每条日程上次用的外观/纸色（`note_open` 时写入、换色时更新），接力时新窗口优先用它自己的记录，否则沿用上一节那张纸 → `note_look_remembered()`。
- **位置持久化**：接力写盘走 `note_save_state()`，于是 `zsched_notes.txt` 里记的就是接力后的位置，重启也在。
- **总开关**：`note_tt_auto_get/set()`，存在 `zsched_notes.txt` **首行** `ttauto=1/0`（老版本读这行会当 id≤0 忽略，不会报错）；关掉后课程照常，只是不自动开关窗口。
- **内容自动调整**：课表便笺的倒计时框显示「距上课 / 距下课 / 本节已下课」+ 第二行「下节 课名 HH:MM」（`note_layout()` 为课表多留了一行高度）；便笺墙上的课表便笺显示「距上课 / 上课中 / 本节已下课」。
- 入口：编辑窗勾选（`IDE_TT`）、列表/表格/便笺墙右键（`IDM_BOARD_TT`）、桌面便笺右键、便笺菜单（含 `IDM_BOARD_TTAUTO` 开关与 `IDM_BOARD_TTNOW` 打开现在这节课）。

### L. 倒计时显示精度：天 / 时 / 分 / 秒（最新一轮）
- 字段 `Schedule.cdUnit`：`0 = CDU_DEF`（未设置，等同「秒」）、`1..4 = 天/时/分/秒`。**默认 0 = 秒** 是为了让旧数据（没有这一行）显示完全不变。
- `cd_parts()` + `cd_text_u(occ, unit, …)` + `cd_text(s, occ, …)`（model.c）：天=`5 天`；时=`5天 03 时`；分=`5天 03:24`；秒=`5天 03:24:41`；粗档位最细一级**向上取整**（差 12 分 → 「1 时」），过期仍是「已过期」。
- 生效位置：表格倒计时列、清单卡片倒计时框、顶部大号数字（改用 `cd_text()`）、便笺墙倒计时行、桌面便笺倒计时框。
- 入口：编辑窗 4 个单选（`IDE_CDU_DAY..SEC`）、右键子菜单（`IDM_CDU_DAY..SEC`，对选中项生效并存盘）、视图菜单（**没有选中任何日程时作用于全部**，`do_set_cd_unit_many()`）。

### M. 修掉「未来的倒数日显示成已过去 0 天」（最新一轮）
- 根因：表格/卡片的纪念日分支写死了「已过去 N 天」，而 `sched_anniv_elapsed()` 对还没到的日期返回 0 → 显示「已过去 0 天」。
- 修法：统一走 `sched_anniv_text()`（还有 / 已过去 / 就是今天），表格填充与每秒刷新两处都改；清单卡片用 `anniv_short()`（短文案）。
- 顺手修掉 `sched_anniv_text()` 自己的两处小毛病：当天（`d == 0`）无论将来与否都显示「就是今天」；**将来**的倒数日不再拼「（距下次 N 天）」（那会和「还有 N 天」自相矛盾）。

---

## 4. 关键决策与踩坑教训

**架构决策**

1. **分层窗口的投影不靠位图资源**：`UpdateLayeredWindow` 先把纸面/纹理/折角/文字全画进 32bpp DIB（GDI 只写 RGB），最后按**几何**逐像素重算 alpha（圆角内 255、外圈按到边界距离渐隐）。好处：文字边缘不会被脏化，也不需要任何阴影图。
2. **纸质纹理程序化生成**：确定性伪随机 96×96 瓦片 + 按底色缓存画刷 + `SetBrushOrgEx` 对齐屏幕，多张卡片像同一种纸。
3. **日期运算用「自然日序号」**绕开时区/夏令时；求下次发生用除法直接跳到目标周期再回退校验（年代久远也不线性循环）。
4. **图片框用千分比（‰）存**，而不是像素：与便笺尺寸解耦，改便笺大小或换 DPI 都不串位。
5. **滚动条单独占一列**（用户明确要求「干脆把滚轮单独列出来一列」），且只在需要时出现；不与照片/正文争夺像素。
6. **滚轮优先级**：`Ctrl+滚轮` = 照片缩放（避免误缩放），普通滚轮 = 便笺内滚动，再兜底翻页。
7. **自研 RTF 而不是用 RichEdit 控件画便笺**：便笺/墙上需要逐 run 自绘（还要跟随便笺缩放比），RichEdit 控件撑不起分层窗口与自绘场景。
8. **图片解码 GDI+ 优先、WIC 回退**：`IWICFormatConverter::Initialize` 在虚表索引 8，目标格式 `GUID_WICPixelFormat32bppPBGRA`，WebP 必须走内存流解码。
9. **提醒用位标志 + 主循环 tick 驱动**，弹窗是独立窗口类（用日程自己的纸色），不阻塞主循环。
10. **名字与图标集中成常量**（`APP_TITLE` / `APP_ICON_ID`），改品牌只动一处 + rc 一行。
11. **自启动注册表值名用 ASCII（`Jian`）**，中文值名在部分环境容易出问题。

12. **课表用「状态机 + 接力」而不是「重建窗口」**：每节课一条日程、各自一份便笺状态，接力时只关旧窗 + 原地开新窗；这样窗口位置/纸色/置顶都由既有的 `zsched_notes.txt` 机制托管，不需要新文件。
13. **倒计时精度用「0 = 未设置 = 秒」而不是「0 = 天」**：新增字段的默认值必须等于老行为，否则老数据一升级显示就变了。
14. **便笺窗口标题 = 日程标题**（便笺纸外观没有标题栏，但 Alt+Tab / 任务栏 / 课表接力时一眼能看出换成了哪一节），也顺便让「验证接力」变得可观测。
16. **数据位置只认一个目录**：四份文件都从“数据目录”派生，换 exe/换版本/换电脑只要目录一样就什么都不丢；
    设置放注册表（不是 exe 旁的文件），所以同一台机器上多份 exe（桌面 + 工程目录）**天然共用同一份数据**，不会再出现两边数据不一致。
17. **导入旧数据 = 切换而不是复制**：`app_data_import()` 只换路径 + 先存档，源目录与目标目录的文件都不动；
    要“搬过去”时另有 `app_data_copy_to()`（备份/导出）——这样用户永远不会因为一次点击丢掉任何一边的数据。
19. **文字颜色存在 RTF 里，不新增字段**：RichEdit 输出的 `\colortbl`/`\cfN` 本身就是标准 RTF，解析/绘制各加一段即可；
    数据文件格式、便笺状态文件都不用动（`rtf=` 那一列本来就存整个 RTF）。
20. **书签是“第三种外观”，不是第三种窗口**：复用便笺的分层窗口 + 纸张绘制 + 状态文件，只换形状与交互；
    切换用“销毁重开”（`note_reopen_layered`），避免在同一个 HWND 上折腾 WS_EX_LAYERED / WS_POPUP 与分层位图。
21. **“单击还原、拖动吸附”靠一个 dragged 标志区分**：位移阈值取 4 逻辑像素（×DPI），比任何时间阈值都稳。
23. **「下一节」名称行的显示优先级**（见 §3-S）：**课名 > 时间 > 其他**——先只按课名挑字号（小号→正文→最小），
    放得下才拼上「什么时候」；框高不够时先**降倒计时字号**、再**让掉日期行**，最后才轮到正文拿剩余空间。
    这样“名称行 + 倒计时”永远在，正文也不会被无理挤掉。
24. **这个名称行是「独立开关」**（`Schedule.ttNext`，1 = 显示 / 0 = 不显示，旧数据按 1 算）：
    与倒计时精度（`cdUnit`）放在编辑窗同一行、便笺右键也有同名项；做成“局部的显示选项”而不是全局设置，
    因为不同课表的便笺大小/需求不一样。
25. **窗口居中要夹紧工作区**：编辑窗长高后（`EW_H` 812 → 872），原来的 `center_over()` 只夹了左上角，底部按钮会被屏幕切掉；现在整窗夹进 `SPI_GETWORKAREA`。
26. **自选铃声不做音频解码**：提醒到点用 `ShellExecuteW(L"open", 文件)` 交给系统默认程序（wav/mp3/m4a/flac 都能响），
    不在 exe 里塞解码器；代价是会拉起播放器，所以**弹窗必须写明「（这是铃声）」**——这也是需求里那句“窗口会提示「这是铃声」”的落点。
27. **新增字段的默认值必须等于老行为，且不轻易动数据格式版本**：`sndmode/sndfile` 两行老数据没有 → 系统提示音（完全不变）；
    旧版本读到未知键会忽略 → 所以 **`ZScheduleData/4` 不用升版本、不用迁移**（升版本反而会触发“旧数据回写”的副作用）。
28. **CSV 导入单独成模块（`csvimport.c`）+ 自带正则引擎**：工具链没有 `<regex.h>`；引擎必须用 **CPS（续接）** 而不是
    “组内整体贪婪”——第一版就是后者，`(a|ab)c` 会静默漏匹配（探针一眼抓到），重写后组/分支/量词都能正确回退。
29. **“只识别便笺支持的字段”怎么落实**：名称 → 标题；时间 → 开始/结束 + 每周重复；
    **其余列一律按「列名：值」写进正文**；时间类列只在“残值还有意义”时才补写正文（`1-16周(单)` 要留，`周一 08:00-09:40` 不重复留）。
30. **铃声播放走 Media Foundation，而不是“打开文件”**：探针确认 MCI/DirectShow 都不吃 flac 而 MF 可以，
    一条路覆盖 wav/mp3/flac；MF 再往下才是 MCI 与 ShellExecute 兜底。**不做事件回调**——铃声短、只要开始和停止。
31. **翻页是“便笺的属性 + 页脚控件”，而不是把墙拆成多个窗口**：一条便笺记 `page=`，墙只摆当前页；
    这样拖动/固定/收起的既有逻辑一行没动，页脚只多做了一次矩形命中。
32. **容量上限用“动态数组 + 扩容”，不预先放大常量**：`note_ensure()` 翻倍扩容、`g_lookMem` 有上限（只是缓存），
    `z[]/rects[]` 跟着条数扩；这样“上限”是真的没有，而不是从 64 变成 1000。
33. **表格搜索用“过滤后重建行”而不是隐藏行**：ListView 的隐藏行在排序/选中/勾选上很容易出交叉问题；
    重建行时**行号必须用独立计数器**（`row`），否则过滤后行号会跳空。
34. **后台模式靠“隐藏主窗口”实现不占任务栏**：`SW_HIDE` 之后任务栏按钮自然消失，不需要 `WS_EX_TOOLWINDOW` 的花招；
    便笺/提醒本来就是独立顶层窗口，照常工作。

**踩过的坑（别重复踩）**

- `maxScroll` **被重复赋值覆盖**导致滚轮恒为 0（两处：stickies.c 里有两段一模一样的滚动计算）。改滚动逻辑时务必确认全文只有**一处**写 `maxScroll`。
- 曾用 `if (vb > avail) vb = avail;` 把内容底边夹到可视区 → `maxScroll` 永远是 0。内容高的自然高度**不能**夹。
- 便笺拖拽只改 `left/top` → 窗口越拖越小，必须保留 w/h。
- `GetTextExtentExPointW` 的 `lpnDx` 对 CJK 不可靠 → 换行用二分 + `GetTextExtentPoint32W`。
- `rtf_to_plain` 的 `BYTE bytes[4]` 太小，长中文段落会被截断（已改成 1024 + 快满就 flush）。
- 照片出现就吞掉正文（`drewBig` 门控）是错的：`drewBig` 只该跳过底部缩略图行。
- **编码纪律**：`build.bat` 必须是 **GBK + CRLF**（用 Python 改它时要用 `newline='\r\n'`，否则 cmd 报「'UT' 不是内部或外部命令」）；其余源文件 UTF-8，`stickies.c` 里的中文字面量全部写成 `\uXXXX` 转义（否则 gcc 执行字符集报错）。
- **补丁脚本纪律**：Python 改文件必须**检测并保留各文件自身编码**；`s = rep(s, ...)` 一定要**赋值**（漏赋值就是静默空操作）；改完用 `findstr` 复核；`py -c` 多行在 cmd 里会静默失败 → 写成脚本文件；PowerShell 内联中文/here-string 会被乱码 → 写成 .ps1 文件。
- **不要动用户自己的文件**：`z3.py`~`z6.py` 是用户写的，`image.c` 里的 `FullEnt` / `img_get_full` 也是用户加的（保留 `IMG_FULL_CACHE` 宏，只加兼容入口 `img_draw_cover_full`）。
- **验证方式**：这台机器上截图经常输给前台竞争（ZCode/浏览器盖住窗口），所以以**数值/文件级验证**为主（临时探针 dump 成 txt/bmp、`findstr` 复核、`certutil -hashfile` 比对），需要看界面时才 `SetForegroundWindow` + 截局部区域。
- **破坏性测试前先备份** `zsched_data.txt / zsched_notes.txt / zsched_board.txt`。测试脚本里一定要把备份**在 finally 里**恢复：有一轮脚本在中途抛异常（解析工作区字符串写错），测试数据就留在了工程目录里，后面几轮又把它当成「原始数据」来备份，最后是从 `zsched_history\` 里挑最新的快照才恢复回来。
- **跨进程给主窗口发菜单命令**（用来开编辑窗/切视图截图）要用 `PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(cmd, hi), lp)`，且**必须先 EnumWindows 找到窗口再发**（窗口句柄为 0 时 PostMessage 静默失败）；`hi=1` + `lp=scheduleId` 能让 `do_edit` 直接开指定日程。
- **`_tools\` 常备测试工具**：`pwin.exe`（列窗口 / 置顶聚焦）、`pmsg.exe`（给主窗口发菜单命令、或给任意类名窗口发消息）、
  `pmouse.exe`（模拟单击/拖拽）、`pmenu.exe`（打印菜单树，含勾选/禁用状态）、`pcrop.ps1`（按真实像素裁屏）。
  看界面时先用 `pwin.exe <pid> ZScheduleMainWnd 1` 把窗口置顶再截图，避免截到被别的窗口遮住的画面。
- **`_tools\` 里 v1.5 新增的测试工具**（都在 `_tools` 目录，编译产物在 `_tools\bin\`，见 `.gitignore`）：
  - `csvprobe.c` + `t1..t6_*.csv`：**离线探针**，`#include "../csvimport.c"` 直接调 static 函数，
    自测正则引擎（含 `(a|ab)c` 回溯、`{2,4}`、`[^0-9]`、中文时刻改写、节次）和 6 组样例 CSV 的识别结果；
    输出 `_tools\csvprobe_out.txt`（UTF-8）。编译命令写在文件头注释里（要 `-ffunction-sections -Wl,--gc-sections`）。
  - `loadprobe.c`：同理 `#include "../model.c"`，验证 `model_load` 是否正确读出 `sndmode/sndfile` 等字段。
  - `testsetup.py` / `testsetup2.py`：造测试数据（`_ttest\zsched_data.txt` + `ring.cmd` + `ring.wav`）
    并把 `HKCU\Software\Jian\DataDir` 指过去；`testrestore.py` **把 DataDir 还原**（原来没有这个值就删掉）——
    **测完一定要跑它**，否则下一次启动会去读测试目录。
  - `pclick.ps1`（DPI 感知的定点真鼠标点击）/ `pkeys.ps1`（SendKeys；**文件对话框里要用剪贴板 `^a`/`^v`**）。
- **`_tools\` 里 v1.6 新增的探针**：
  - `audioprobe.c`：一次性验 MCI / DirectShow / **Media Foundation** 对 wav/mp3/flac 的支持（结论见 §3-V 表格）；
    编译要 `-lwinmm -lole32 -lstrmiids -lmfplat -lmfreadwrite -lmfuuid`，文件头有 `STRSAFE_NO_DEPRECATE`。
  - `soundprobe.c`：`#include "../sound.c"` 直接调 `snd_play_file()` / `snd_stop()`，打印实际走的通道。
  - 测试用音源临时从本机音乐里复制过（`ring.mp3` / `ring.flac`），**测完已删**，别把大文件留在工程里。
- **验证“铃声是程序内播的”**：到点后看有没有**新的播放器进程**（`Get-Process` 按 StartTime 过滤）——
  没有就说明走的是 MF；再对照 `soundprobe` 打印的 channel 1。
- **验证课表接力**别靠截图：同时看 ①`pw.exe` 枚举出的便笺窗口标题（= 日程标题）与矩形 ②`zsched_notes.txt` 里的 `id=`，两者一起判断「关旧窗 / 原位开新窗」最可靠。
- **验证“自选铃声真的响了”**：v1.5 的做法（`.cmd` 留痕文件 / 看是否拉起播放器）在 v1.6 起不再适用——
  现在是程序内 MF 播放，判据换成“**没有新的播放器进程**”+ 弹窗上的 `铃声：xxx（这是铃声）`。
- 多行 `py -c "..."` 在 cmd 里会因引号被吃掉而静默失败（本次又踩了两次）→ 一律写成 `.py` 脚本文件。
- **数据文件里的 RTF 反斜杠是双份**：写盘时 `esc_append` 会把 `\` 转义成 `\\`，读盘时 `unesc_into` 再还原。
  造测试数据时如果只写单份，`rich_parse` 会因为开头对不上 `{\rtf` 而**静默退回纯文本**（正文显示正常、但字号/字体/颜色全丢）——
  本次第一版测试数据就踩了这个坑，看起来像“颜色功能没生效”。
- **`\colortbl` 解析别重复推进下标**：`{\colortbl;\red..;\red..;}` 里分号只是分隔符，
  第一版“遇到颜色项 +1、遇到分号再 +1”会让第 2、3 项各错一格（表现为第二段显示成黑色），正确做法是**只在空项时 +1**。
- **测试进程不 DPI 感知 → 坐标被 ×1.5**：用 `PostMessage` 发鼠标消息、`SetCursorPos`/`mouse_event` 注入真实点击时，
  如果**测试进程**没有 `SetProcessDPIAware()`，系统会把坐标按 DPI 缩放（150% 下 ×1.5），于是“点了没反应”“扫不到手型光标”。
  这次在这一点上绕了很久：最后用“post 一个已知 lParam 再看 App 收到什么”的办法一眼看穿（发的 100,100 收到的是 150,151）。
  `_tools` 里的 `pmsg/pmouse/pwin` 都已加 `SetProcessDPIAware()`；写测试脚本时也要在开头调一次。
- **删临时调试代码别用“非贪婪正则”**：`\{ */* TEMP DEBUG */.*?^\}` 这种会在**内层**大括号处停下，
  把 debug 块连同别人的 `}` 一起删掉（编译报 “expected identifier before 'if'”）。改这种结构用手写精确匹配，或者删完务必编译一遍。
- **别把 `ctl_text`/类似的 setter 当 getter 用**：`ui_edit.c` 里 `ctl_text()` 是写控件、`ctl_int()` 才是读控件，
  混用不会编译报错、界面也“看起来正常”，只是用户输入被静默丢掉（本次的“提醒次数改了不生效”就是这么来的）。
  写这类小工具函数时名字里最好带上方向（set_/get_），或者干脆不提供这种同名的读写对。
- **跨进程 `SetWindowTextW` 对控件不可靠**：从测试工具里 `SetWindowTextW(GetDlgItem(编辑器, 3065), L"3")`
  看起来成功（连回读都是新值），但**控件所属进程自己读到的仍是旧值** —— 这次排查在这一点上绕了很久。
  结论：验证带输入框的界面要用**真鼠标 + 真键盘**（`pmouse.exe … clickctl <id>` 点控件 + `keybd_event` 输入），
  或者干脆用程序自己的接口。
- **测试数据的枚举值要对**：`REP_WEEKLY = 1`（不是 2；2 是 `REP_CUSTOM`）。有一轮测试把 `rep=2` 当“每周”，
  结果跑出来是“每天”，白排查了一轮。写测试数据前先看 `zsched.h` 里的枚举。
- **`WM_INITMENUPOPUP` 的菜单句柄在 `wParam`，不是 `lParam`**：写成 `lParam` 不会报错、不会崩，只是那个分支永远不执行——
  这类“静默不生效”的坑只能靠日志确认（本次加一行 `fprintf` 就一眼看穿）。
- **菜单项状态别在 `WM_CREATE` 里算一次就完事**：那时数据/便笺状态往往还没载入，算出来的可用性与文案都是错的；
  正确做法是在 `WM_INITMENUPOPUP` 里按当前状态刷新。
- **用 git 提交给旧版本“打包备份”**：本机没装 git，导出用 dulwich 遍历某次提交的文件树再写盘（见 §6）；
  注意 `Tree.items()` 返回的是 **`(name, mode, sha)` 三元组**（写成二元组会 `ValueError: too many values to unpack`），
  目录用 `mode & 0o040000` 判断并递归。
- **测试脚本崩溃会留下脏数据**：有一轮脚本在中途抛异常（元组解包写错），测试数据就留在工程目录里，后面几轮又把它当“原始数据”备份。
  写这种脚本时把恢复动作放进 `finally`，或跑完检查一遍数据文件里的标题。
- **造测试数据时，数据文件里的路径必须双写 `\`**：手写 `sndfile=C:\...\_ttest\ring.cmd`（单份）会被 `unesc_into` 把 `\r` 当回车、`\_` 把反斜杠吃掉，
  表现是编辑窗里铃声名变成一串没有斜杠的怪字、`GetFileAttributesW` 找不到文件 → **提醒只响系统提示音**。
  应用自己写盘是 `esc_append`（双写），手写测试数据务必同样双写。
- **`refresh_from_disk()`（文件 → 重新载入）在读失败时会 `model_seed_sample()`**：本轮测试踩到过一次——
  刚写完数据文件就发重载命令，`model_load` 失败 → **内存里的两条测试日程被换成演示数据**（后续那次回写也失败了，所以文件没被覆盖）。
  也就是说：**一次瞬时读取失败 = 当前数据被演示数据顶掉**（还可能连带把演示数据写进数据文件）。这是既有隐患，
  已记入 §5 待办（本轮没动它，避免超出需求范围）。
- **探针要 `#include` 被测模块**：`_tools/csvprobe.c` / `loadprobe.c` 直接 `#include "../csvimport.c"`（或 `../model.c`），
  这样能调用里面的 **static 函数**；再用 `-ffunction-sections -fdata-sections -Wl,--gc-sections` 编译，
  没被 `main` 用到的函数会被丢掉，于是不必为 `csv_import_show` 里引用的 `note_* / palette_color` 准备实现
  （只补了几个数据符号 `g_hInst` / `g_palette` 与 `img_is_image` 这类叶子函数）。纯逻辑先过探针，比截图/发消息快一个数量级。
- **文件对话框不能用 SendKeys 打字**：Windows 文件对话框的自动完成会把逐字输入吃掉（实测只剩 `ring.`，然后报“找不到文件”）。
  稳妥做法：`Set-Clipboard` 放路径 → 点一下「文件名」框 → `^a` `^v` → 点「打开」（`_tools/pkeys.ps1` + `_tools/pclick.ps1`）。
- **1.5 倍 DPI 的机器上截图/点击一律用物理像素**：这台是 3840×2160 @150%（`SM_CXSCREEN` 返回物理值；
  PowerShell 不设 DPI 感知时读到的是虚拟化后的 2560×1440，会算错窗口位置）。`pcrop.ps1` / `pclick.ps1` / `pkeys.ps1` 都调了 `SetProcessDPIAware()`。
- **空行是“一节课的边界”**：扫网格单元格时**一次只吃一个换行**——`while (*e == '\n' || *e == '\r') e++;` 会把空行一起吞掉，
  于是空行分隔的两门课被粘成一门（探针里那 5 节课变成 4 节就是这么来的）。
- **量词/组的回溯不能靠“组内整体贪婪”**：第一版匹配器里组匹配完就直接接组的后继，失败时不会回到组内换一种长度——
  `(a|ab)c` 匹配 `abc` 会 MISS。改成续接（CPS）后正常；这类问题**只有写针对性用例才会暴露**，
  所以探针里专门放了 `(a|ab)c`、`[0-9]+5`、`[^0-9]`、`{2,4}` 这几条。
- **给提醒排期做测试时，`remind=7` 三种方式全开更好排查**：弹窗（看得出文案）、通知栏、提示音；
  提示音那一路只需把 `sndfile` 指到一个“会留痕”的文件（v1.5 用 `.cmd` 写标记文件）就能确认调用链真的跑了；
  v1.6 起是程序内播放，**判据换成“到点有没有冒出外部播放器进程”**（没有 = 走的是 MF）。
- **MinGW 下用 MF / DirectShow 头文件要先 `#define STRSAFE_NO_DEPRECATE 1`**：`dshow.h` 会带进 strsafe，
  它把 `wcscpy/wcscat/swprintf` 统统替换成“请改用 StringCch*”的报错宏；`_CRT_SECURE_NO_WARNINGS` 不管用。
- **MF 的接口在 C 里只能走 `obj->lpVtbl->Method(obj, ...)`**：`IMFSourceReader_ReadSample` 这类“宏”是 C++ 的；
  另外 `IMFPresentationDescriptor::GetStreamDescriptorByIndex(this, index, &selected, &sd)` 是 4 个参数，
  `IMFMediaSession::Start(this, timeFormat, &PROPVARIANT)` 的第二个参数是 `PROPVARIANT*`（`GUID_NULL` 不行）。
- **页过滤要“归一化后比较”**：`bp->page > 1 && bp->page != s->page` 会把“还没设过页（0/1）”的便笺当成任何页都算，
  于是第 2 页空着也显示「本页已满」。正确写法是 `st_item_page(id) != s->page`（0/1 归一成 1）。
  这个 bug 是截图看出来的（第 2 页明明是空的，按钮还是红的）。
- **测试“跨进程改控件文本”会踩老坑**：`pmsg ctl <pid> ... searchme` 之后控件显示是 “searchme”，
  但**应用进程自己 `GetWindowTextW` 读到的还是旧值**（v1.0 就记过这条）。真实键盘输入才作数；
  另外这台机器开着中文输入法，`SendKeys` 打英文会先进入**输入法组字状态**（候选窗弹出、`EN_CHANGE` 没提交），
  要再按一次 Enter 提交组字才会触发过滤。测试搜索时别看第一次截图就下结论。
- **临时调试代码要按大括号配对删**：本轮用“找 TEMP DEBUG 行 + 数 { } 配对”的脚本删（正则删不掉嵌套块）。
  调试输出统一写 `zsched_cmdlog.txt`（已在 .gitignore 忽略），**收尾一定要 `grep TEMP DEBUG` 复核**。
- **“80 张便笺”测试别用 Stop-Process 杀进程**：`note_save_state()` 只在正常退出（WM_CLOSE）时写盘，
  强杀会得到“0 张恢复”的假结果；正确姿势是发 `WM_CLOSE(16)` 让它自己收尾。

---

## 5. 未完成待办 / 待确认

按优先级排列：

1. **需用户实机确认（最高优先）** —— 这些只有用户能判断手感：
   - **迁移功能**：点一次「迁移：在桌面生成数据拷贝」，看桌面上那个文件夹的内容和 `迁移说明.txt` 是否够清楚；
     如果更希望“迁到 U 盘/指定盘”，说一声就把目标改成选文件夹（见第 8b 条）。
   - **mp3 / flac 铃声**：用自己的音乐文件设一次提醒，听一下是不是程序内直接响（不该弹出播放器）；
     嫌不够响/想调音量的话，MF 那条路目前没接音量（要加得给音频渲染器设属性，记在下面第 4 条）；
   - **便笺墙翻页**：一页贴满后点「本页已满 · 新开一页」是不是顺手；`PgUp/PgDn` 和右键「移到下一页」够不够用；
   - **表格搜索**：关键词、`Esc` 清空、`Ctrl+F` 的手感；要不要把“周表/清单”也加上搜索；
   - **后台运行**：托盘图标在不在溢出区、左键切换是否直觉；要不要加“关闭主窗口时默认收进托盘”。
   - **CSV 课表导入**：拿自己学校的课表导出试试（表格型/网格型），确认识别出的名称列、时间、正文分行是否合意；
     「导入并打开便笺」一次最多开 16 张会不会太多；要不要把「其余信息」也做成可勾选的（现在一律进正文）；
   - 便笺墙与桌面便笺的**滚轮 / 独立滚动条**是否够用、是否遮挡照片；
   - 桌面便笺上**逐段字体/字号**放大后的观感；照片**独立缩放 + 便笺缩放**两者是否会混淆；
   - **课表模式**：到点自动开关便笺、原位接力与「下节」文案（可用编辑窗把一节排到 2 分钟后自查）；
   - **课表便笺的「下一节」名称行**：位置/字号观感、点它跳转是否顺手。
2. **`refresh_from_disk()` 的隐患（发现但本轮未修）**：读数据失败时它会直接 `model_seed_sample()`，
   等于**一次瞬时读取失败就把内存里的数据换成演示数据**（若接下来回写成功，还会把演示数据写到数据文件里；
   好在 `model_save()` 前有 `model_archive()`，历史目录里还能捞回来）。建议改法二选一：
   ① 读失败时**什么都不做**（保留当前内存数据）并提示“重新载入失败”；
   ② 读失败时先试一次 `model_archive()` 存旧文件，再决定是否 seed。
   —— 触发场景：数据文件刚被外部程序写完、被短暂占用时点「重新载入」。
3. **提醒状态的持久化**：`g_rem` 只在内存里，程序重启后「已提醒次数/是否已提醒」会重来。若用户希望跨重启续算 → 落盘。
4. **CSV 导入可再补的**：① 直接读取 `.xlsx`（要自己解 zip + XML）；② 把「节次 → 时间」的作息表做成可编辑
   （现在硬编码 `g_periodStart[12]`）；③ 导入预览里支持取消勾选某些行；④ 便笺墙（`stickies.c`）上的课表便笺
   目前仍是紧凑单行提示（§3-U 只做在清单/便笺上）。
5. **动图播放**：GIF / 动画 WebP 目前只画首帧；要做逐帧需要 `GdipImageGetFrameCount` / `FrameDimensionTime` + 定时器重绘。
6. **manifest 品牌残留**：`zsched.manifest` 里 `assemblyIdentity name="ZSchedule.Desktop.Schedule"` 与
   `<description>ZSchedule 桌面日程管家</description>` 仍是旧名（功能无影响，纯观感；要改就改这两行）。
7. **`zsched_res.o`** 是构建产物（187,668 字节），留在目录里；如需可加个清理/`.gitignore`。
8. ~~**便笺数量上限**~~：v1.6 已移除（窗口动态数组、启动恢复全部、`do_note_all` 也按实际条数分配）。
8b. **迁移功能的延伸**：现在固定生成到桌面；如果用户希望“迁移到 U 盘/指定盘”，
    可以在同一个函数上换成 `pick_folder()` 起点（或者做成两个菜单项）；另外可以考虑在拷贝里附带 `笺.exe`，做成自包含的迁移包。
9. **可选增强**：历史版本目前只能「恢复最近一份 + 打开文件夹手动拷」；如需列表对话框选择任意一份，可在 `model_history_*` 上加一层枚举。
10. **便笺墙（`stickies.c`）上没有「下一节」名称行**：这一功能只做在**独立便笺弹窗**上（用户当时强调的就是弹窗）。
10b. **铃声音量**：MF 播放没接音量（想加的话：给音频渲染器 `IMFActivate` 设 `MF_AUDIO_RENDERER_ATTRIBUTE_...`，
    或直接用 `IMFSimpleAudioVolume` —— 需要 `MFCreateAudioRenderer` 那条路，而不是现在的拓扑节点）。
10c. **后台运行的记忆**：`g_background` 只在本次运行里有效；如果用户希望“关掉窗口就进托盘”，
    可以在 `WM_CLOSE` 里按一个持久化开关决定，别直接改默认行为（会吓人）。
10d. **翻页的交互增强**：可以加“拖动便笺到页脚左右区域就翻页”、或“一页满时新建日程自动翻页”；本轮只做了明确的按钮/快捷键。
11. **备份/发版留档**：目前是最新版（v1.5）一份备份（见 §0/§6）。若用户希望“每版都留一份”，
    可把“导出提交 → 编译 → 写备份说明”做成 `_tools\backup_version.py <commit> <目标目录>`；
    注意用户要的是**最新版**的备份。
12. **课表模式的已知边界**（都不是 bug，必要时再改）：
    - 时间窗按「**今天**的这一次发生」算，**跨午夜**的课（如 23:50–00:30）不会接力；
    - 只有普通日程能标课表；无时间待办/纪念日即使标了也会被 `model_normalize()` 清掉；
    - 同一时刻有多节课时取「最近开始的」那一节显示；
    - 课程表本身没有专门的「周课表编辑界面」，仍是一条日程一节课（每周重复）。

---

## 6. 继续工作的操作手册

```bat
rem 1) 编译前先关掉正在运行的实例（否则链接失败）
taskkill /IM 笺.exe /F
rem 2) 构建
cd /d C:\Users\Crychic\.zcode\workspace\default\ZSchedule
build.bat
rem 3) 同步到桌面（用户日常用的是桌面那份）
copy /Y 笺.exe C:\Users\Crychic\Desktop\笺.exe
```

- 改完务必**检查编译输出是否零 error / 零 warning**（`-Wall -Wextra` 全开）。
- 改了 `zsched.rc` / `app.ico` 后确认 `windres` 真的重跑（`zsched_res.o` 的时间戳要更新）。
- 品牌相关字符串只应出现在 `APP_TITLE` / `APP_ICON_ID` / `zsched.rc` 三处，别散落硬编码。
- **版本记录（git）**：本机没装 git，仓库是用 dulwich（纯 Python）建的，位于工程目录 `.git`：
  ```bat
  py _tools\git_init.py                 rem 首次：建仓库 + 首次提交
  py _tools\git_init.py "修复：xxx"      rem 之后：暂存改动 + 提交一轮
  ```
  提交历史用 `git log` 看（装好 git 后可直接读同一个 `.git`）；给人看的版本记录是 `CHANGELOG.md`。
  每轮收尾顺序：编译验证 → 更新 `CHANGELOG.md` / `README.md` / `progress.md` → `py _tools\git_init.py "..."` → 同步桌面 exe。

- **给某个旧版本打包一份独立备份**（v1.4 / v1.5 都是这么做的）：
  1. 用 dulwich 遍历目标提交的文件树写到新目录（`Tree.items()` 是 `(name, mode, sha)` 三元组；
     `mode & 0o040000` 是子目录要递归；`repo[sha].data` 是文件内容）；
  2. 在新目录里跑 `build.bat`（它会自动找 `..\tools\w64devkit`）→ 得到可运行的 `笺.exe`；
  3. 写一份 `备份说明.md`：来自哪个提交、与当前版本的差异、数据是否兼容、怎么运行/编译。
  4. 数据文件（`zsched_*.txt`）**不要**放进备份——它们是你的日程；版本之间数据格式兼容
     （v1.5 只多两行 `sndmode=`/`sndfile=`，旧版读到会忽略）。
- 验证清单（改 UI 后跑一遍）：图标能 `ExtractAssociatedIcon` 取出、窗口标题 = 笺、托盘 tip = 笺、数据文件路径 = exe 同目录。
  改了 `csvimport.c` 就再跑一次 `_tools\csvprobe.exe`（编译命令见该文件头注释）看 `csvprobe_out.txt` 有没有回归。

---

## 7. 最近一轮的验证记录（证据）

| 项 | 结果 |
| --- | --- |
| 构建（本轮 v1.9.4） | `笺.exe` **743,863 字节**，零 error / 零 warning；MD5 `3eb2c2bc7995995869ed1a6ee49e470c` |
| **复制到新开的一页（截图）** | 选中 A → 工具栏「复制」→「新开一页」→「粘贴」：页 2/2 出现 A 的副本并选中；`zsched_board.txt` 为 `viewpage=2 pages=2 id=4 … page=2`（`_tools/shots/v194/paste_page2.png`）|
| **重启后页归属仍在** | 重启程序停在 2/2 页，页 2 只有那张粘贴的便笺（`restart_page2.png`）|
| **表格视图粘贴** | 表格视图按 `Ctrl+V`：自动切回清单视图，新便笺出现在当前页并被选中（`table_paste.png`）|
| **空剪贴板置灰** | 工具栏「粘贴」、编辑菜单、右键菜单都置灰（`editmenu_empty.png` / `ctx_empty.png`）|
| **搜索框不被截走** | 搜索框里 `Ctrl+V` 粘贴的是系统剪贴板文字，没有新增日程（`search_cv.png`）|
| **关于（截图）** | 版本 `v1.9.4`、作者「祝筱笺」、GitHub 蓝色超链接 + 「打开项目主页 / 关闭」；Esc 关闭（`about.png`）|
| 构建（v1.9.3，上一轮） | `笺.exe` **741,371 字节**，零 error / 零 warning；MD5 `fb78b62d65990d4c941bb4bed4d996ee` |
| **滚到最底正文可见（截图）** | 620×620 便笺、10 行正文、竖图：顶部照片 + 第一行（`_tools/shots/v193/note_scroll_top.png`）；滚到底**第十行贴在便笺底边**（`note_scroll_bottom.png`）；滚回顶部恢复（`note_scroll_back.png`）|
| **照片放大仍留一行** | `imgb0=0,0,1000,300`（满宽照片）后底部仍有一行正文；往下滚可见其余正文；拖照片本体停在正文最后一行上方（`note_drag_clamp.png`）|
| **便笺墙卡片** | 卡片滚到底最后一行可见（`card_scroll_bottom.png`）；上滚时内容裁在视口内、不压标题 |
| **回归** | 纯正文便笺滚动正常（`note_text_only.png`）；纯照片便笺照片仍占满正文区（`note_photo_only.png`）|
| **用户真实便笺（CMC）** | 正文是 `\n\n高等代数…`：顶部照片 + **实际文字行可见**（`real_cmc_v193_top.png`）；滚轮往下文字仍在（`real_cmc_v193_bottom.png`）；滚回顶部恢复（`real_cmc_v193_back.png`）|
| 构建（v1.9.2，上一轮） | `笺.exe` **739,835 字节**，零 error / 零 warning；MD5 `52f3d895fe494b170aafd1c1e4568b8e` |
| **指针离开照片不消失（截图）** | 独立便笺 400×1200 竖图：指针停在照片上（`_tools/shots/v192/note_hover.png`）→ **无手柄**；移到便笺外 (60,950)（`note_leave.png`）→ **照片仍在**、位置不变（v1.9.1 会整张消失）|
| **点选才进编辑** | 单击照片（`note_select.png`）：虚线框 + 四角手柄，**严丝合缝贴住照片**；悬停时没有任何手柄（对照 `note_hover.png`）|
| **右键退出编辑** | 选中后右键（`note_rclick.png`）：手柄/虚线框消失、**照片保留**、便笺菜单正常弹出；`Esc` 关菜单后照片仍在 |
| **拖角 / 拖本体** | 拖右下角向内：`imgb0` `376,308,230,691` → `376,308,159,482`（左上不动、宽高同比例）；拖本体：`378,259,159,482`（只有 x/y 变，w/h 完全不变）|
| **双击进编辑窗** | 双击照片 → `ZScheduleEditWnd\|编辑日程` 出现，`Esc` 取消后便笺正常 |
| **便笺墙卡片同一口径** | 清单视图：悬停照片无手柄（`card_hover.png`）；单击出手柄且贴住照片（`card_select.png`）；右键取消选中 + 卡片菜单（`card_rclick.png`）|
| **框被最小尺寸撑开（诊断 + 修复）** | 日志实测 `cw=390 wf=400 slot=283 → w=94 h=283`，却被 `minW=156` 撑成 156 宽（照片 94 宽，左右各空 31px）；删除两处布局钳制后框与照片严丝合缝（独立便笺同样）|
| **v1.9.2 备份包** | `py _tools\export_commit.py HEAD ..\ZSchedule_backup_v1.9.2_20261017` 导出 53 个文件 → 在备份目录 `build.bat` 编译成功；三份 `笺.exe`（工程 / 桌面 / 备份）**MD5 完全相同**（`52f3d895fe494b170aafd1c1e4568b8e`，739,835 字节）✓ 备份 = 当前最新版 |
| **用户真实数据上确认** | 桌面实例（`C:\Users\Crychic\Desktop\笺.exe`）启动后，「CMC数竞初赛」便笺照片正常显示（`_tools/shots/v192/real_note_cmc.png`）——原先指针一离开就消失的就是这张 |
| 构建（v1.9.1，上一轮） | `笺.exe` **739,330 字节**，零 error / 零 warning；MD5 `94d0563465ad0539c0e235908ad5e4b6` |
| **墙上照片完整显示（截图）** | 宽图 1200×300 在 340×300 卡片里整张可见（红框 + 四角色块都在），框与照片同比例、没有白边；竖图同理（`_tools/shots/v191/`）|
| **墙上四角缩放** | 拖右下角：`imgb0` `0,522,1000,496` → `0,522,839,413`（左上角不动、宽高同比例）；Ctrl+滚轮后 `imgb0` 不变 |
| **墙上双击编辑** | 双击卡片照片 → `ZScheduleEditWnd\|编辑日程` 打开（取消后正常）|
| **拖动只改位置** | 独立便笺拖本体：`0,300,1000,400` → `0,628,1000,400`（w/h 完全不变，只有 y 变）|
| **照片完整显示（截图）** | 宽图/竖图 × 扁便笺/窄高便笺四种组合：照片整张可见（四角色块、红框、对角线都在），框与照片严丝合缝（无框内空白）；便笺无正文时照片占满正文区（截图 `_tools/shots/v19/*.png`）|
| **四角双向缩放** | 拖右下角：`imgb0` 从 `121,193,464,795` → `121,193,387,635`（左上角不动、宽高同比例变）；拖左上角：`340,296,378,652`（x/y 跟着走、宽高同比例）；照片始终不变形、整张可见 |
| **单击拖动 / 双击编辑** | 拖照片本体：`215,193,464,795` → `123,155,464,795`（只改位置，尺寸不变）；双击照片 → 出现 `ZScheduleEditWnd\|编辑日程`，取消后正常 |
| **光标** | `GetCursorInfo` 实测：照片左上/右下角 = `SIZENWSE`，照片中间 = `SIZEALL`，便笺空白处 = `SIZEALL` |
| **便笺自身手柄** | 拖便笺右下角：窗口 1028×488 → 1090×530（照片四角优先，两者共存）|
| **迁移：文件夹与内容** | 菜单「迁移：在桌面生成数据拷贝」→ 桌面出现 `笺数据迁移_20261017_165716\`；内容：`zsched_data.txt` / `zsched_notes.txt` / `zsched_board.txt` / `zsched_history\` / `迁移说明.txt`；三个数据文件与源文件 **MD5 完全一致**；历史目录里 3 个快照都在（修复后）|
| **迁移：说明文件** | `迁移说明.txt` 以 UTF-8+BOM 写出，内容为「笺 · 数据拷贝（迁移用）/ 生成时间 / 四个文件各是什么 / 怎么迁到新电脑（放 exe 或选数据文件夹）/ 注意事项」，可直接用记事本打开 |
| **迁移：同一秒重复** | 同时发两次命令 → `笺数据迁移_20261017_165727` 与 `笺数据迁移_20261017_165727_2` 并存，第一个没被覆盖 |
| **历史目录复制的老 bug** | 探针复现：源 `_ttest\zsched_history` 有 2 个文件，而 `app_data_copy_to` 拷出来的历史目录是空的，并打印出它把源路径拼成了 `_ttest\zsched_xxx.txt`（少了 `zsched_history\`）；改 `histDir` 后同一探针显示 `copied=1`，实际拷贝里历史快照齐全 |
| 构建（v1.7，上一轮） | `笺.exe` **734,062 字节**，零 error / 零 warning；MD5 `0670688b2af7c0e12f1549b010a22e83` |
| **接力时间点（有课间）** | 数学课 16:45:59–16:46:11、英语课 16:46:50 开始（中间 39 秒空档）。每秒轮询窗口标题：16:46:12（下课 +1）…16:46:14（+3）仍是 **数学课**，16:46:16（**+5**）已变成 **英语课**；全程 **只有 1 个便笺窗口**；`zsched_notes.txt` 只有一条 `id=2`，窗口矩形与前一节完全相同（原位换窗） |
| **接力时间点（连堂）** | 数学课 16:47:57–16:48:07、英语课 16:48:07–16:48:27（首尾相接）。16:48:08…16:48:11（end+4）仍是数学课，**16:48:12（end+5）换成英语课**，`n=1` 全程单窗；英语课下课后（16:48:27 + 5）→ 16:48:32 窗口关闭且**没有**开出明天/别处的课（`zsched_notes.txt` 只剩 `ttauto=1`）|
| **提示音停止小窗（出现）** | 只勾提示音的日程到点：出现 `ZScheduleRingWnd|铃声提醒|3309,1866,495,186`（495×186 物理 = 330×124 逻辑 @150%）；`pmsg get` 读到按钮文本 **`停止铃声`**；截图 `_tools/shots/v17/v17_ring_stop.png`（纸色蓝、标题「长铃声-手动停止」） |
| **提示音停止小窗（手动）** | `pmouse clickctl 1` 点「停止铃声」→ 窗口立刻消失（`pwin` 不再列出 `ZScheduleRingWnd`），声音停下 |
| **提示音停止小窗（自动）** | 1.2 秒的短 wav：16:47:15 小窗出现 → **16:47:17 自己关掉**（无人点击），即“按 MF 时长判断播完”生效 |
| 构建（v1.6，上一轮） | `笺.exe` 730,122 字节，零 error / 零 warning；MD5 `e685d44541cbf168e056b1c9d899d534` |
| **播放通道探针** | `_tools/audioprobe.exe`（wav/mp3/flac 三格式 × MCI/DirectShow/MF）：wav 三路都 OK；mp3 三路都 OK；**flac 只有 MF OK**（MCI err 263、DShow 0x80040216 / 0x80040265）；`_tools/soundprobe.exe` 走 `snd_play_file()`：wav/mp3/flac 全部返回 **channel 1（Media Foundation）**，`snd_stop()` 后 active=0 |
| **mp3 铃声（真机）** | 到点提醒：弹窗出现（`日程提醒 / 数学课 MATH-101 / 2026-10-17 15:19:04 不重复 / 铃声：ring.mp3（这是铃声）`）+ 通知栏气泡；**进程列表里没有任何播放器进程**（v1.5 会拉起 Microsoft.Media.Player）→ 走的是程序内 MF |
| **flac 铃声（真机）** | 第二条提醒（`英语课 FLAC`）到点：弹窗显示 **`铃声：ring.flac（这是铃声）`**（截图 `_tools/shots/v16/popup_flac.png`），同样**没有外部播放器进程**；点弹窗关闭后声音停止（WM_DESTROY → snd_stop） |
| **便笺容量（80 张）** | 造 80 条日程 → 「为全部待办开便笺」→ `pwin` 数到 **80 个 `ZScheduleStickyNote` 窗口**（旧版 64 上限会截断）；`WM_CLOSE` 正常退出 → `zsched_notes.txt` 161 行 → 重启 **80 张全部恢复**（旧版只恢复 24 张） |
| **便笺墙翻页** | 12 条便笺时页脚显示 `‹ 第 1 / 1 页 ›` + 朱红实心 **「本页已满 · 新开一页」**（截图 `_tools/shots/v16/p1_footer_full.png`）；点它 → `第 2 / 2 页`、空页提示「这一页还没有便笺…」、按钮变回描边的 **「＋ 新一页」**（`p2_page_empty.png`，此处修掉过一个“空页也显示已满”的 bug）；点 `‹` 回第 1 页；`PgUp` 同样生效；点 `›` 到第 2 页（`p2_footer.png`） |
| **翻页持久化 / 移页** | 新开页后 `zsched_board.txt`：`viewpage=2` + `pages=2`；发 `IDM_PAGE_MOVENEXT`（`lp=1 hi=1`）→ 文件变成 `pages=2` + `id=1 … page=2`（视图在 1 页时 `viewpage` 不写）；此时第 2 页出现「数学课 MATH-101」、第 1 页少一张 |
| **表格搜索** | 表格视图顶部出现搜索框（提示「搜索：标题 / 正文 / 链接（Esc 清空）」）；输入 `searchme` 后列表从 12 行筛成 10 行（测试数据的备注里都有该词）、状态栏显示 **「搜索「searchme」：10 条」**（截图 `search_filtered.png` / `status_search_count.png`）；`Esc` 清空后恢复 12 行 |
| **后台运行** | 发 `IDM_FILE_BACK`：`pwin` 再也列不出主窗口（任务栏按钮消失），但**托盘溢出区有「笺」图标**（截图 `tray_and_popup_bg.png`）；后台状态下**提醒弹窗与通知栏照常弹出**；点托盘图标 → 主窗口 `ZScheduleMainWnd|笺|114,114,1242,886` 回来 |
| **菜单项** | `pmenu` 实测：文件菜单 `id=2043 后台运行（不占任务栏）`；视图菜单 `id=2095 上一页(PgUp)` / `2096 下一页(PgDn)` / `2097 新开一页` |
| **测试环境已还原** | `HKCU\Software\Jian\DataDir` 已删除（测试前就没有）；`_ttest\` 已删；工程目录的 `zsched_notes.txt` / `zsched_board.txt` 测试期间挪开过、已原样放回 |
| **v1.6 备份包** | `py _tools\export_commit.py HEAD ..\ZSchedule_backup_v1.6_20261017` 导出 49 个文件 → 备份目录里 `build.bat` 编译成功；三份 `笺.exe`（工程 / 桌面 / 备份）**MD5 完全相同**（`e685d44541cbf168e056b1c9d899d534`，730,122 字节）|
| **v1.7 备份包** | `py _tools\export_commit.py HEAD ..\ZSchedule_backup_v1.7_20261017` 导出 49 个文件 → 备份目录里编译成功；工程 / 桌面 / 备份三份 `笺.exe` **MD5 相同**（`0670688b2af7c0e12f1549b010a22e83`，734,062 字节）|
| **git** | v1.6 `fe69d39a`；v1.7 `4121d162`；v1.8 `bb744616`；v1.9 `2ccc4d37`；v1.9.1 `a60ad60f`；v1.9.2 `af0802ef`；v1.9.3 `e7edb989` + `66569386`；v1.9.4 `efa01ff5`；`_tools/shots/`（证据截图）、`_tools/bin/`、`*_out.txt` 均被 `.gitignore` 忽略 |
| **v1.9.1 备份包** | `py _tools\export_commit.py HEAD ..\ZSchedule_backup_v1.9.1_20261017` 导出 53 个文件 → 备份目录编译成功；工程 / 桌面 / 备份三份 `笺.exe` **MD5 相同**（`94d0563465ad0539c0e235908ad5e4b6`，739,330 字节）|
| 构建（v1.5，上一轮） | `笺.exe` 629,353 字节，零 error / 零 warning；`certutil -hashfile` MD5 `60a89c286917a0fffcb58eaacc8bc51f` |
| **正则引擎（离线探针）** | `周一 08:00-09:40`→ 命中 `周一`(g1=一)；`星期五 10:00`→ g1=五；`1-16周(单)`→ **不误伤**（周次不是星期）；`TUE 8:00`→ g2=TUE（忽略大小写）；`08:00-09:40`→ 两个时刻 8:00 / 9:40；`第3-4节`→ p1=3/p2=4；`1、2节`→ p1=1/p2=2；`2026-10-16`→ 日期；`^[0-9]+$` 对 `12345` 命中、对 `12a45` 不命中；**量词回溯** `[0-9]+5` 命中 `12345`；**分支回溯** `(a\|ab)c` 命中 `abc`（第一版实现这里是 MISS，重写成续接后命中）；`[^0-9]` 对 `abc` 命中、对 `a1c` 不命中；`{2,4}` 对 `12` 命中、对 `12345` 不命中；中文时刻改写 `8点30分到10点` → `8:30分到10:00` |
| **CSV 识别（离线探针，6 组样例）** | ① 表格型（表头+时刻列）：识别 2 节，名称列=课程名称、时间列=上课时间，正文 3 行（教师/教室/学分），周一 8:00-9:40 / 周三 10:00-11:40，周掩码 2 / 8；② 星期+节次列：`线性代数 周二`，节次 `第3-4节` → **10:00-11:40**（按内置作息表推），正文含 `周次：1-16周` 与 `节次：第3-4节（时间按内置作息表推导）`；③ 网格型：3 行 × 4 列 → **5 节**（含空行分隔出的一格两门课），时间由节次推；④ GBK + 分号 + 全角数字：`星期二 １４：００－１５：４０` → 周二 14:00-15:40；⑤ 无表头：名称列按内容推断正确（课名权重 > 教师名）；⑥ 带日期列：`2026-11-20` → 一次性日程（rep=0），时间为 14:00-16:00 |
| **铃声（编辑窗）** | 「铃声…」按钮 + 右侧 `铃声：ring.cmd` 显示正确（`pmsg get` 读控件文本 + 截图 `_tools/shots/shots_v15_ringrow.png`）；点按钮弹出菜单：`系统提示音（Windows 默认）` / `✓自选声音文件` / `选择声音文件…` / `试听（打开所选文件）` / `清除自选文件` / 灰字提示（截图 `shots_v15_ringmenu.png`）；点「系统提示音」→ 控件立刻变成 `铃声：系统提示音`，点「确定」后数据文件里 `sndmode=0`（`sndfile` 路径仍保留，方便切回来） |
| **铃声（真机到点）** | 测试数据两条日程都设 `remind=7`（弹窗+通知栏+提示音）：第一条 `sndfile=_ttest\ring.cmd` —— **到点 00:03:46 时标记文件 `ring_opened.txt` 出现**（内容 `ring-opened`），证明 `ShellExecute` 真的打开了所选文件；弹窗截图显示 `日程提醒 / 数学课 / 2026-10-17 00:03:46 不重复` + **`铃声：ring.cmd（这是铃声）`**（朱印红）+ 正文，窗口高度比普通提醒高（540×381 vs 540×357 @150%）；第二条 `sndfile=ring.wav` 到点 00:04:46 → **`Microsoft.Media.Player` 进程随之出现**（已关闭），说明 `.wav` 由系统默认播放器打开 |
| **CSV 导入（真机集成）** | 菜单「文件 → 导入 CSV 课表…」→ 系统文件对话框（标题「选择要导入的 CSV 课表文件」、过滤「课表文件 (*.csv;*.txt)」）→ 识别框（截图 `shots_v15_csvwnd.png`）：`格式：表格（一行一节课）　表头：第 1 行　列：5（分隔符「,」）`、`名称列：课程名称　时间列：上课时间　其余 3 列写入正文`、`共 2 节课可导入` + 预览 `1. 高等数学 周一 8:00-9:40 · 正文 3 行`；点「导入并打开便笺」→ 数据文件里出现 `高等数学`（`rep=1 week=2 tt=1 cd=3`，start=2026-10-19 08:00）与 `大学英语`（`week=8`，start=2026-10-21 10:00），正文分别是 `教师：张三 / 教室：教一101 / 学分：4`；桌面便笺窗口出现两张（标题 `高等数学` / `大学英语`），便笺上显示「2026-10-19 08:00:52 每周 周一 / 还有 2天 07:54 / 教师：张三…」（截图 `shots_v15_note.png`） |
| **CSV 导入（网格型识别框）** | 同一入口选 `t3_grid.csv` → 识别框显示 `格式：课表网格（行 = 节次、列 = 星期）`、`共 3 行 × 4 列，识别出 5 节课`、`共 5 节课可导入　· 节次按内置作息表推导时间`，预览 5 条（含空行分出的 `物理实验` / `大学物理` 两条）——确认后取消，未改动数据（截图 `shots_v15_csvgrid.png`） |
| **数据兼容** | 新字段 `sndmode/sndfile` 落盘/回读正确（`loadprobe` 报告 `sndMode=1 sndFile=C:\...\_ttest\ring.cmd pathExists=1`）；老数据（无这两行）载入后 `sndmode=0`，行为与 v1.4 一致；`ZScheduleData/4` 版本号未变 |
| **测试环境已还原** | `HKCU\Software\Jian\DataDir` 已删除（测试前本来就没有这个值），`_ttest\` 测试目录已删；本轮只留 `_tools\` 里的探针/样例 CSV/脚本与 5 张证据截图（`_tools\shots\`，`.gitignore` 已忽略） |
| **v1.5 备份包** | `py _tools\export_commit.py HEAD ..\ZSchedule_backup_v1.5_20261017` 导出 47 个文件 → 在备份目录里 `build.bat` 编译成功；三份 `笺.exe`（工程 / 桌面 / 备份）**MD5 完全相同**（`60a89c286917a0fffcb58eaacc8bc51f`，629,353 字节）✓ 备份 = 当前最新版 |
| **git** | 本轮提交 `c98cf362`（24 个文件入库，8 项被 `.gitignore` 忽略：`笺.exe` / `zsched_res.o` / 数据三件 / `zsched_history\` / `_tools\bin\` / `_tools\shots\`）|
| 构建（v1.4，上一轮） | `笺.exe` 593,462 字节，零 error / 零 warning；「下一节」名称行 / 悬停 / 点击跳转 / 独立开关 / 每秒刷新均已验证（详见上一版文档） |
| **「下一节」名称行（截图）** | 课表便笺倒计时下方显示 `下一节 英语精读与写作 22:51`；课名长时先丢掉时间、课名完整不被截断（`…（第二课堂）` 那种长名字实测完整显示） |
| **悬停反馈（截图）** | 鼠标移到名称行 → 文字变蓝并加下划线（光标同时变手型，`WM_SETCURSOR` 按 `ttHot`） |
| **点击跳转（集成）** | 点名称行后编辑窗打开；用「课表：显示下一节课名称」开关识别改到的是哪条：保存后 `ttnext` 变成 `['ttnext=1','ttnext=0']`，即**第 2 条（英语…）= 下一节课**那条 ✓（不是便笺自己的“概论”） |
| **独立开关（截图）** | 关掉 `IDM_NOTE_TTNEXT` 后名称行消失、倒计时框变矮；再打开恢复（数据文件里 `ttnext=1`） |
| **每秒刷新** | 秒级倒计时（`cdunit=秒`）相隔 6 秒的两张便笺截图**字节不同**（倒计时在走）；对照组：分精度下相隔 5 秒两张**完全相同** |
| **最新版备份** | 从 git 最新提交（HEAD，v1.4）导出 34 项到 `..\ZSchedule_backup_v1.4_20261016\`，在备份目录里编译成功；里面的 `笺.exe` 与工程、桌面三份**MD5 完全相同**（`c32fc6491f1277e7e66748666bcbf061`，593,462 字节）——即备份确实是当前最新版 |
| **收起/摆回（用户现场数据复现 + 修复后）** | 起始 `id=2`（日程已删的残留记录）`+ id=6/7` 全 `hid=1`；修复后发 `2034`：两张都摆回、残留记录被清空（文件变空）；只收起 `id=6` 时文件只剩 `id=6 … hid=1`；“收起 ⇄ 摆回”来回切正常 |
| **提示语误报（截图）** | 只收起一张时，便笺墙正常显示另一张卡片（“元旦节”）且**不再**出现“便笺都收起来了” |
| **菜单状态刷新（日志 + 菜单树）** | 调试日志实测 `WM_INITMENUPOPUP` 的 `lp=0x3`（说明不是句柄）；改用 `wParam` 后 Alt+V 打开视图菜单：`id=2034` 从 `[x]`（灰）变为**可用**、`id=2035` 变为「显示全部便笺」 |
| **git 仓库** | dulwich 建立 `.git`，首次提交 `d21455b1`：33 个文件入库，7 项被 `.gitignore` 忽略（`笺.exe` / `zsched_res.o` / `zsched_data.txt` / `zsched_notes.txt` / `zsched_board.txt` / `zsched_history\` / `_tools\bin\`） |
| **逐段颜色（模型探针）** | `rich_parse` 对 `{\colortbl;\red198\green44\blue44;\red42\green92\blue204;\red28\green146\blue76;}` + `\cf1..\cf3`：颜色表 4 项（colors[1]=198/44/44、[2]=42/92/204、[3]=28/146/76），分段 4 段且 color=1,2,3,0，文本正确 |
| **逐段颜色（截图）** | 桌面便笺正文：`RED-DAY`红、`BLUE-DAY`蓝、`GREEN-DAY`绿、`DEFAULT-COLOR`默认色；便笺墙卡片：`红色`红、`蓝色`蓝、`绿色`绿、`默认`默认色；编辑窗载入同一段 RTF 也逐段上色（往返一致） |
| **编辑窗颜色调色板（截图）** | 「正文」行：字体/字号/B/I/U/整篇应用 之后是 7 枚色块（第 1 枚画白斜杠＝默认自动色） + 「颜色…」，与下方正文框不重叠 |
| **书签形态（集成）** | 开启后窗口变成 360×51 的长条并贴在**顶边**（当时离上边最近），`zsched_notes.txt` 记下 `style=2 … edge=2 rstyle=1 rx=300 ry=260 rw=520 rh=360`；拖拽（600,300）后重新吸在顶边 x=900；**单击（无位移）后窗口精确回到原来的 297,257,538×378**，状态文件也回到 `style=1`；再次变书签后**重启仍是书签**（300,0,368×59，edge=2） |
| **书签外观（截图）** | 鹅黄纸色长条 + 内侧深色台 + 圆角 + 一端挂绳孔 + 横排标题，贴在屏幕顶边 |
| **统一/自定义数据目录（模型探针）** | 未设置时 `app_data_dir()` = exe 目录；`app_data_find()` 扫到 3 份：`%APPDATA%\Jian`（2 条・便笺 1）、`Desktop`（2 条）、工程目录（6 条），按修改时间新→旧；`app_data_copy_to()` = 1（主数据 + 历史目录都到位）；`app_data_dir_set()` 写注册表并生效；切到备份目录后载入 6 条（旧版会叠成 10 条，已修） |
| **UI：自定义目录生效（集成）** | 把 `DataDir` 指向测试目录后启动：① 便笺窗口标题 = 测试目录里那条日程（说明数据 + 便笺状态都读的是新目录）；② 菜单标签 `id=2017 当前：<测试目录>\`；③ 发送存档命令后**测试目录的 mtime 变、工程目录的不变**，且新目录里出现 `zsched_history\` |
| **UI：导入对话框（截图）** | 列出三行：`2 条日程 · 便笺 1 条 · 2026-10-16 00:35 %APPDATA%\Jian`、`2 条 · 便笺 0 条 · 00:26 Desktop`、`6 条 · 便笺 无 · 2026-10-15 13:29 工程目录`；取消后窗口正常关闭、程序仍在运行 |
| **UI：选文件夹对话框** | 点「选择数据文件夹…」弹出系统 `#32770|浏览文件夹`，关掉后程序正常 |
| 倒计时精度（模型探针） | 差 3天5:23:41 → 天档`4 天`、时档`3天 06 时`、分档`3天 05:24`、秒档`3天 05:23:41`；差 12:07 → 天档`1 天`；过期 → `已过期` |
| 倒数日文案（模型探针） | 将来 +100 天 → `还有 100 天`；过去 66 天 → `已过去 66 天（距下次 299 天）`；当天 → `就是今天` |
| 课表判定（模型探针） | `sched_class_now()` → 数学课；`sched_class_next()` → 英语课（没标课表的「体育课」被排除）；明天的课今天取窗口 = 无；没填结束时间 = 45 分钟；待办强制标课表 → `timetable=0` |
| **课表接力（集成）** | 测试数据：数学课 [T-20s, T+40s)、英语课 [T+40s, T+180s)。启动 +5s：便笺窗口标题 = **数学课**，矩形 43,26,312,270；+45s（下课后）：**窗口只剩 1 个、标题已变成英语课、矩形完全相同**；`zsched_notes.txt` 在同一时刻写成 `id=2 style=1 color=0 top=1 x=68 y=42 w=450 h=387` ✓ |
| 课表便笺内容（截图） | 倒计时框两行：`距下课 00:30` + `下节 英语课 00:46`，正文与标题正常，无裁切 |
| 便笺墙课表文案（截图） | 课表便笺显示 `上课中 00:30`；普通便笺按精度显示 `还剩 5天 03 时` |
| 表格倒数日（截图） | 未到来的纪念日 → **`还有 100 天`**；过去的 → `已过去 66 天`；`cdunit=天` 的日程 → `6 天`；`cdunit=秒` → `5天 02:59:…` |
| 编辑窗（截图） | 底部两行新增：`课表 ☑ 标记为课表：到点自动开关便笺、内容跟随上下课`、`倒计时 ○天 ○时 ○分 ●秒 显示到该精度（课表默认到分）`；删除/便笺/取消/确定 四个按钮都在工作区内（编辑窗实测 521,263,1188,1348，底边 1611 ≤ 工作区 2088） |
| 保存字段 | 存档后数据文件里 `timetable=`、`cdunit=` 两行正确落盘（`timetable=1/0`、`cdunit=0/2`） |
| 命令通路 | `PostMessage(WM_COMMAND 2006)` → 立刻出现 `zsched_history\zsched_20261016_001836.txt`（跨进程发菜单命令可用） |
| 图标 / 名称（上一轮） | `ExtractAssociatedIcon` 32×32 ✓；窗口 `[日程便笺]` `[笺]`；桌面与工程两份 `笺.exe` 字节相同 |
| 滚动 / 富文本（更早） | 溢出便笺 `maxScroll = 129 / 211`；`face[1]=华文楷体`、`halfPt=32` |

---

## 8. 源码规模（行数，便于定位）

| 文件 | 行数 | 职责 |
| --- | --- | --- |
| `ui_main.c` | 4086 | 主窗口、工具条、倒计时面板、清单/表格视图、**提醒（含自选铃声）**、**表格搜索**、**托盘/后台运行**、**课表接力 `tt_tick()`**、**数据保存位置与导入对话框**、菜单 |
| `stickies.c` | 2898 | 便笺墙（笔记本页）：拖动/固定/缩放/滚动/照片、课表文案、**翻页（页码栏 / 新开一页 / 每页归属）** |
| `noteview.c` | 2792 | 桌面便笺窗口：分层窗口、阴影、拖拽缩放、换色换肤、滚动、照片、外观记忆、书签形态、**动态窗口表（无上限）** |
| `model.c` | 2312 | 数据模型、周期推算、纪念日、倒计时精度、课表时间窗、数据目录与导入、存读（含 `sndmode/sndfile`）、历史存档 |
| `csvimport.c` | 2141 | CSV 课表导入：极简正则引擎（CPS 匹配器）+ 编码/分隔符/引号处理 + 表格型/网格型识别 + 确认框 + 生成课表便笺 |
| `ui_edit.c` | 1730 | 编辑窗（提醒行 + 「铃声…」来源选择 / 试听 / 停止、待办、便笺图片、课表勾选、倒计时精度单选、正文颜色调色板） |
| `weekview.c` | 1078 | 周 × 小时网格 + 缩放 |
| `richtext.c` | 793 | RTF 存取 + 逐 run 自绘（字体 / 字号 / 字形 / 颜色）+ 字体枚举 |
| `image.c` | 694 | GDI+/WIC 解码、缩略图与原图缓存、cover 绘制、位置/大小换算 |
| `zsched.h` | 634 | 公共类型、常量（`APP_TITLE` / `APP_ICON_ID` / `TT_CLASS_MIN` / `CDU_*` / `SND_SYS|SND_FILE` / `SNDV_*` / `NOTE_STYLE_BOOKMARK` / `NOTE_EDGE_*` / `MAX_IMGS` / `IMGM_*` / `IMGL_*` / `REM_*`）、菜单/控件 id、模块接口 |
| `imgview.c` | 466 | 原图查看器 |
| `paper.c` | 388 | 纸质感绘制原语 |
| `sound.c` | 233 | **铃声播放**：Media Foundation（SourceResolver + 音频渲染拓扑）→ MCI → 系统默认程序；start/stop/is_active（按时长判断播完） |
| `main.c` | 103 | WinMain、初始化、消息循环（退出时 `snd_shutdown`） |
