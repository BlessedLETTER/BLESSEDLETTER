/* ============================================================================
 *  ZSchedule - image.c
 *  图片加载 / 缩略图 (GDI+) 与普通文件图标绘制
 *
 *  说明：这里手工声明了所需的 GDI+ 扁平 API，避免依赖 gdiplus.h（C++ 头文件）
 *        或各发行版是否提供 gdiplusflat.h，链接时只需 -lgdiplus。
 * ==========================================================================*/
#include "zsched.h"

#if defined(_M_IX86) || defined(__i386__)
#  define GDPIAPI __stdcall
#else
#  define GDPIAPI
#endif

typedef int   GpStatus;
typedef void  GpImage;
typedef void  GpBitmap;
typedef void  GpGraphics;
typedef DWORD GpARGB;

/* GdiplusStartupInput 的字段顺序/类型必须与官方头文件一致 */
typedef struct {
    UINT32 GdiplusVersion;
    void  *DebugEventCallback;
    BOOL   SuppressBackgroundThread;
    BOOL   SuppressExternalCodecs;
} GpStartupInput;

__declspec(dllimport) GpStatus GDPIAPI GdiplusStartup(ULONG_PTR *token,
                                                      const GpStartupInput *input,
                                                      void *output);
__declspec(dllimport) void GDPIAPI GdiplusShutdown(ULONG_PTR token);
__declspec(dllimport) GpStatus GDPIAPI GdipCreateBitmapFromFile(const WCHAR *filename,
                                                                GpBitmap **bitmap);
__declspec(dllimport) GpStatus GDPIAPI GdipGetImageWidth(GpImage *image, UINT *width);
__declspec(dllimport) GpStatus GDPIAPI GdipGetImageHeight(GpImage *image, UINT *height);
__declspec(dllimport) GpStatus GDPIAPI GdipGetImageThumbnail(GpImage *image, UINT tw, UINT th,
                                                             GpImage **thumb,
                                                             void *callback, void *data);
__declspec(dllimport) GpStatus GDPIAPI GdipCreateFromHDC(HDC hdc, GpGraphics **graphics);
__declspec(dllimport) GpStatus GDPIAPI GdipDeleteGraphics(GpGraphics *graphics);
__declspec(dllimport) GpStatus GDPIAPI GdipSetInterpolationMode(GpGraphics *graphics, int mode);
__declspec(dllimport) GpStatus GDPIAPI GdipDrawImageRectI(GpGraphics *graphics, GpImage *image,
                                                          INT x, INT y, INT w, INT h);
__declspec(dllimport) GpStatus GDPIAPI GdipDrawImageRectRectI(GpGraphics *graphics, GpImage *image,
                                                              INT dx, INT dy, INT dw, INT dh,
                                                              INT sx, INT sy, INT sw, INT sh,
                                                              int unit, void *attrs,
                                                              void *callback, void *data);
__declspec(dllimport) GpStatus GDPIAPI GdipDisposeImage(GpImage *image);

/* --------------------------------------------------------------- 缩略图缓存 */
#define IMG_CACHE 96
#define IMG_THUMB_MAX 160
#define IMG_FULL_CACHE 24      /* 原分辨率缓存条数（便笺照片用） */

typedef struct {
    wchar_t  path[MAX_PATH];
    GpImage *img;
    int      w, h;
    void    *owned;      /* WIC 像素缓冲，随条目一起释放 */
} ImgEntry;

static ImgEntry  g_cache[IMG_CACHE];
static int       g_cacheNext;
static int       g_cacheCount;
static ULONG_PTR g_token;
static int       g_ready;

#ifdef ZS_WIC_TRACE
#  include <stdio.h>
#  define WIC_TRACE(what, hr, ptr) \
      fprintf(stderr, "wic: %-26s hr=0x%08lX ptr=%p\n", what, (unsigned long)(hr), (void *)(ptr))
#else
#  define WIC_TRACE(what, hr, ptr) ((void)0)
#endif

/* 统一解码入口（定义在后面）：先 GDI+，失败再走 WIC */
static GpImage *load_any(const wchar_t *path, int *pw, int *ph, void **owned);

/* WIN32_LEAN_AND_MEAN 会挡掉 ole 头，这里手工声明需要的 COM 入口 */
__declspec(dllimport) HRESULT WINAPI CoInitializeEx(void *pvReserved, DWORD dwCoInit);
__declspec(dllimport) void    WINAPI CoUninitialize(void);
__declspec(dllimport) HRESULT WINAPI CoCreateInstance(const GUID *rclsid, void *pUnkOuter,
                                                     DWORD dwClsContext, const GUID *riid,
                                                     void **ppv);

/* --------------------------------------------------------------- 原图缓存 */

typedef struct {
    wchar_t  path[MAX_PATH];
    GpImage *img;
    int      w, h;
    void    *owned;
} FullEnt;

static FullEnt g_full[IMG_FULL_CACHE];
static int     g_fullNext;

int img_init(void)
{
    GpStartupInput in;
    memset(g_cache, 0, sizeof(g_cache));
    memset(&in, 0, sizeof(in));
    in.GdiplusVersion = 1;
    if (GdiplusStartup(&g_token, &in, NULL) != 0) {
        g_ready = 0;
        return 0;
    }
    g_ready = 1;
    return 1;
}


void img_shutdown(void)
{
    int i;
    for (i = 0; i < IMG_CACHE; i++) {
        if (g_cache[i].img) GdipDisposeImage(g_cache[i].img);
        if (g_cache[i].owned) free(g_cache[i].owned);
        g_cache[i].img = NULL;
        g_cache[i].owned = NULL;
    }
    for (i = 0; i < IMG_FULL_CACHE; i++) {
        if (g_full[i].img) GdipDisposeImage(g_full[i].img);
        if (g_full[i].owned) free(g_full[i].owned);
        g_full[i].img = NULL;
        g_full[i].owned = NULL;
    }
    if (g_ready) {
        GdiplusShutdown(g_token);
        g_ready = 0;
    }
}

int img_is_image(const wchar_t *path)
{
    static const wchar_t *exts[] = {
        L"png", L"jpg", L"jpeg", L"jpe", L"jfif", L"bmp", L"gif",
        L"tif", L"tiff", L"ico", L"wmf", L"emf",
        /* 新增：走 WIC 解码器（系统装了扩展就能看） */
        L"webp", L"heic", L"heif", L"avif", L"jxl", L"dds", L"exif",
        L"raw", L"arw", L"cr2", L"nef", L"dng", L"pcx", L"tga", L"pbm",
        L"pgm", L"ppm", L"pnm", L"wbmp", L"wdp", L"hdp", L"jxr", NULL
    };
    const wchar_t *dot = NULL, *p;
    int i;
    if (!path) return 0;
    for (p = path; *p; p++) {
        if (*p == L'.') dot = p;
        else if (*p == L'\\' || *p == L'/') dot = NULL;
    }
    if (!dot || !dot[1]) return 0;
    for (i = 0; exts[i]; i++) {
        const wchar_t *a = dot + 1, *b = exts[i];
        int eq = 1;
        while (*a && *b) {
            wchar_t ca = *a, cb = *b;
            if (ca >= L'A' && ca <= L'Z') ca += 32;
            if (cb >= L'A' && cb <= L'Z') cb += 32;
            if (ca != cb) { eq = 0; break; }
            a++; b++;
        }
        if (eq && !*a && !*b) return 1;
    }
    return 0;
}

/* 取（并缓存）缩略图；失败返回 NULL */
static ImgEntry *img_get(const wchar_t *path)
{
    int i, slot;
    GpImage  *thumb = NULL;
    void     *keepOwned = NULL;
    UINT w = 0, h = 0;
    ImgEntry *e;

    if (!g_ready || !path || !path[0]) return NULL;

    for (i = 0; i < IMG_CACHE; i++) {
        if (g_cache[i].img && !wcscmp(g_cache[i].path, path)) return &g_cache[i];
    }
    {
        void *owned0 = NULL;
        GpImage *full = load_any(path, (int *)&w, (int *)&h, &owned0);
        if (!full || w == 0 || h == 0) {
            if (full) GdipDisposeImage(full);
            if (owned0) free(owned0);
            return NULL;
        }
        if (w > IMG_THUMB_MAX || h > IMG_THUMB_MAX) {
            if (GdipGetImageThumbnail(full, IMG_THUMB_MAX, IMG_THUMB_MAX,
                                      &thumb, NULL, NULL) == 0 && thumb) {
                GdipDisposeImage(full);
                if (owned0) free(owned0);
                GdipGetImageWidth(thumb, &w);
                GdipGetImageHeight(thumb, &h);
            } else {
                thumb = full;
                keepOwned = owned0;
            }
        } else {
            thumb = full;
            keepOwned = owned0;
        }
    }

    slot = -1;
    for (i = 0; i < IMG_CACHE; i++) {
        if (!g_cache[i].img) { slot = i; break; }
    }
    if (slot < 0) {                       /* 满了：淘汰最早的一个 */
        slot = g_cacheNext;
        if (g_cache[slot].img) GdipDisposeImage(g_cache[slot].img);
        if (g_cache[slot].owned) free(g_cache[slot].owned);
        g_cacheNext = (g_cacheNext + 1) % IMG_CACHE;
    } else {
        g_cacheCount++;
    }
    e = &g_cache[slot];
    memset(e->path, 0, sizeof(e->path));
    {
        size_t n = wcslen(path);
        if (n >= MAX_PATH) n = MAX_PATH - 1;
        memcpy(e->path, path, n * sizeof(wchar_t));
    }
    e->img = thumb;
    e->w = (int)w;
    e->h = (int)h;
    e->owned = keepOwned;
    return e;
}

/* 取得原图（不缩放）；用于“显示原图”查看器与大图预览 */
static FullEnt *img_get_full(const wchar_t *path)
{
    int i, slot;
    GpImage *orig = NULL;
    void    *keepOwned2 = NULL;
    UINT w = 0, h = 0;
    FullEnt *e;

    if (!g_ready || !path || !path[0]) return NULL;
    for (i = 0; i < IMG_FULL_CACHE; i++)
        if (g_full[i].img && !wcscmp(g_full[i].path, path)) return &g_full[i];

    {
        void *owned0 = NULL;
        GpImage *full = load_any(path, (int *)&w, (int *)&h, &owned0);
        if (!full || w == 0 || h == 0) {
            if (full) GdipDisposeImage(full);
            if (owned0) free(owned0);
            return NULL;
        }
        orig = full;
        keepOwned2 = owned0;
    }

    slot = -1;
    for (i = 0; i < IMG_FULL_CACHE; i++)
        if (!g_full[i].img) { slot = i; break; }
    if (slot < 0) {
        slot = g_fullNext;
        if (g_full[slot].img) GdipDisposeImage(g_full[slot].img);
        if (g_full[slot].owned) free(g_full[slot].owned);
        g_fullNext = (g_fullNext + 1) % IMG_FULL_CACHE;
    }
    e = &g_full[slot];
    memset(e->path, 0, sizeof(e->path));
    {
        size_t n = wcslen(path);
        if (n >= MAX_PATH) n = MAX_PATH - 1;
        memcpy(e->path, path, n * sizeof(wchar_t));
    }
    e->img = orig;
    e->w = (int)w;
    e->h = (int)h;
    e->owned = keepOwned2;
    return e;
}

/* 原图像素尺寸；0 表示无法读取 */
int img_size(const wchar_t *path, int *pw, int *ph)
{
    FullEnt *e = img_get_full(path);
    if (!e) return 0;
    if (pw) *pw = e->w;
    if (ph) *ph = e->h;
    return 1;
}

/* 把原图缩放到指定矩形绘制；smooth=0 时用最近邻（放大时保持像素锐利） */
void img_draw_zoom(HDC hdc, const wchar_t *path, int x, int y, int w, int h,
                   int smooth)
{
    FullEnt *e = img_get_full(path);
    GpGraphics *g = NULL;
    if (!e || w <= 0 || h <= 0) return;
    if (GdipCreateFromHDC(hdc, &g) != 0 || !g) return;
    GdipSetInterpolationMode(g, smooth ? 2 /* HighQuality */ : 5 /* NearestNeighbor */);
    GdipDrawImageRectI(g, e->img, x, y, w, h);
    GdipDeleteGraphics(g);
}

int path_is_image(const wchar_t *path) { return img_is_image(path); }

__declspec(dllimport) GpStatus GDPIAPI GdipCreateBitmapFromScan0(INT width, INT height,
                                                                 INT stride,
                                                                 int pixelFormat,
                                                                 BYTE *scan0,
                                                                 GpBitmap **bitmap);

/* ======================================================================
 *  WIC 回退解码
 *  GDI+ 只认 BMP/JPEG/PNG/GIF/TIFF/ICO；WebP、HEIC、AVIF 等要靠 Windows
 *  成像组件（含商店里的"Webp 图像扩展"）解码。这里手工对接 WIC 的 COM
 *  虚表，把结果转成 32bppPBGRA 像素缓冲，再交给 GDI+ 包成位图，
 *  后续绘制代码完全复用。
 * ====================================================================*/
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateDecoderFromStream)(void *self,
        void *stream, const GUID *vendor, DWORD metadataOptions, void **decoder);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetFrame)(void *self, UINT index, void **frame);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateFormatConverter)(void *self, void **conv);
typedef HRESULT (STDMETHODCALLTYPE *PFN_ConvInit)(void *self, void *src,
        const GUID *dstFmt, int dither, void *palette, double alpha, int paletteType);
typedef HRESULT (STDMETHODCALLTYPE *PFN_GetSize)(void *self, UINT *pw, UINT *ph);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CopyPixels)(void *self, const void *rc,
        UINT stride, UINT bufsize, BYTE *buf);
typedef ULONG   (STDMETHODCALLTYPE *PFN_Release)(void *self);

#define WIC_VT(p)  (*(void ***)(p))
#define WIC_FN(p, idx, type) ((type)(WIC_VT(p)[idx]))

/* {CACAF262-9370-4615-A13B-9F5539DA4C0A} */
static const GUID kCLSID_WICImagingFactory =
    { 0xCACAF262, 0x9370, 0x4615, { 0xA1, 0x3B, 0x9F, 0x55, 0x39, 0xDA, 0x4C, 0x0A } };
/* {EC5EC8A9-C395-4314-9C77-54D7A935FF70} */
static const GUID kIID_IWICImagingFactory =
    { 0xEC5EC8A9, 0xC395, 0x4314, { 0x9C, 0x77, 0x54, 0xD7, 0xA9, 0x35, 0xFF, 0x70 } };
/* {6FDDC324-4E03-4BFE-B185-3D77768DC90F} = GUID_WICPixelFormat32bppPBGRA */
static const GUID kGUID_32bppPBGRA =
    { 0x6FDDC324, 0x4E03, 0x4BFE, { 0xB1, 0x85, 0x3D, 0x77, 0x76, 0x8D, 0xC9, 0x10 } };

#define WIC_PIXFMT_32BPPPARGB   0x000E200Bu

__declspec(dllimport) void *WINAPI SHCreateMemStream(const BYTE *pInit, UINT cbInit);

/* 用 WIC 解码；成功返回可绘制位图，并通过 *owned 交回需随图像释放的缓冲
 *  注意：CreateDecoderFromFilename 对 WebP 这类扩展名会找不到解码器，
 *        所以统一“读入内存 → CreateDecoderFromStream”，靠内容嗅探识别格式。 */
static GpImage *wic_load(const wchar_t *path, int *pw, int *ph, void **owned)
{
    void *factory = NULL, *stream = NULL, *decoder = NULL, *frame = NULL, *conv = NULL;
    GpBitmap *gbmp = NULL;
    BYTE *buf = NULL, *fileData = NULL;
    UINT w = 0, h = 0;
    HRESULT hr;
    HANDLE hf;
    DWORD size, rd = 0;

    *pw = *ph = 0;
    *owned = NULL;

    /* 读文件 */
    hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return NULL;
    size = GetFileSize(hf, NULL);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 256u * 1024u * 1024u) {
        CloseHandle(hf);
        return NULL;
    }
    fileData = (BYTE *)malloc(size);
    if (!fileData) { CloseHandle(hf); return NULL; }
    if (!ReadFile(hf, fileData, size, &rd, NULL) || rd != size) {
        free(fileData);
        CloseHandle(hf);
        return NULL;
    }
    CloseHandle(hf);

    hr = CoCreateInstance(&kCLSID_WICImagingFactory, NULL, 1 /*INPROC_SERVER*/,
                          &kIID_IWICImagingFactory, &factory);
    if (FAILED(hr) || !factory) goto done;

    stream = SHCreateMemStream(fileData, rd);
    if (!stream) goto done;
    hr = WIC_FN(factory, 4, PFN_CreateDecoderFromStream)(factory, stream, NULL,
                                                         1 /*OnLoad*/, &decoder);
    WIC_TRACE("CreateDecoderFromStream", hr, decoder);
    if (FAILED(hr) || !decoder) goto done;

    hr = WIC_FN(decoder, 13, PFN_GetFrame)(decoder, 0, &frame);
    WIC_TRACE("GetFrame", hr, frame);
    if (FAILED(hr) || !frame) goto done;

    hr = WIC_FN(factory, 10, PFN_CreateFormatConverter)(factory, &conv);
    if (FAILED(hr) || !conv) goto done;

    /* 虚表索引 8 = IWICFormatConverter::Initialize
       （与 wincodec.h 中的声明顺序一致：CopyPixels 之后即 Initialize） */
    hr = WIC_FN(conv, 8, PFN_ConvInit)(conv, frame, &kGUID_32bppPBGRA,
                                       0 /*NoDither*/, NULL, 1.0 /*alphaThreshold*/,
                                       0 /*Custom*/);
    WIC_TRACE("ConverterInitialize", hr, NULL);
    if (FAILED(hr)) goto done;

    hr = WIC_FN(conv, 3, PFN_GetSize)(conv, &w, &h);
    WIC_TRACE("GetSize", hr, NULL);
    if (FAILED(hr) || w == 0 || h == 0) goto done;
    if ((double)w * (double)h > 64.0 * 1000.0 * 1000.0) goto done;   /* 过大不载入 */

    buf = (BYTE *)malloc((size_t)w * (size_t)h * 4);
    if (!buf) goto done;
    hr = WIC_FN(conv, 7, PFN_CopyPixels)(conv, NULL, w * 4, w * h * 4, buf);
    WIC_TRACE("CopyPixels", hr, NULL);
    if (FAILED(hr)) { free(buf); buf = NULL; goto done; }

    if (GdipCreateBitmapFromScan0((INT)w, (INT)h, (INT)(w * 4),
                                  WIC_PIXFMT_32BPPPARGB, buf, &gbmp) != 0 || !gbmp) {
        free(buf);
        buf = NULL;
        goto done;
    }
    *pw = (int)w;
    *ph = (int)h;
    *owned = buf;                     /* 缓冲区必须与位图同生共死 */

done:
    if (conv)    WIC_FN(conv, 2, PFN_Release)(conv);
    if (frame)   WIC_FN(frame, 2, PFN_Release)(frame);
    if (decoder) WIC_FN(decoder, 2, PFN_Release)(decoder);
    if (stream)  WIC_FN(stream, 2, PFN_Release)(stream);
    if (factory) WIC_FN(factory, 2, PFN_Release)(factory);
    if (fileData) free(fileData);
    return (GpImage *)gbmp;
}

int img_wic_available(void);
int img_wic_available(void)
{
    void *factory = NULL;
    HRESULT hr;
    hr = CoCreateInstance(&kCLSID_WICImagingFactory, NULL, 1,
                          &kIID_IWICImagingFactory, &factory);
    if (factory) WIC_FN(factory, 2, PFN_Release)(factory);
    return SUCCEEDED(hr);
}

/* 统一入口：先 GDI+，失败再用 WIC（WebP 等） */
static GpImage *load_any(const wchar_t *path, int *pw, int *ph, void **owned)
{
    GpBitmap *orig = NULL;
    UINT w = 0, h = 0;
    *pw = *ph = 0;
    *owned = NULL;
    if (GdipCreateBitmapFromFile(path, &orig) == 0 && orig) {
        GdipGetImageWidth((GpImage *)orig, &w);
        GdipGetImageHeight((GpImage *)orig, &h);
        if (w && h) { *pw = (int)w; *ph = (int)h; return (GpImage *)orig; }
        GdipDisposeImage((GpImage *)orig);
    }
    return wic_load(path, pw, ph, owned);
}

/* 在 (x,y,cx,cy) 内等比缩放居中绘制 */
void img_draw(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy)
{
    ImgEntry *e = img_get(path);
    double sx, sy, sc;
    int dw, dh, dx, dy;
    GpGraphics *g = NULL;

    if (!e || cx <= 0 || cy <= 0) return;
    sx = (double)cx / (double)e->w;
    sy = (double)cy / (double)e->h;
    sc = (sx < sy) ? sx : sy;
    if (sc > 1.0) sc = 1.0;                       /* 小图不放大，保持清晰 */
    dw = (int)(e->w * sc + 0.5);
    dh = (int)(e->h * sc + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    dx = x + (cx - dw) / 2;
    dy = y + (cy - dh) / 2;
    if (GdipCreateFromHDC(hdc, &g) != 0 || !g) return;
    GdipSetInterpolationMode(g, 6 /* HighQualityBicubic */);
    GdipDrawImageRectI(g, e->img, dx, dy, dw, dh);
    GdipDeleteGraphics(g);
}

/* 填满 (x,y,cx,cy)：等比缩放到“覆盖”整个矩形，多余部分裁掉（便笺上贴照片的感觉） */
void img_draw_cover(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy)
{
    FullEnt *e = img_get_full(path);    /* 用原尺寸，保证放大后也清晰 */
    double sx, sy, sc;
    int dw, dh;
    int sw, sh, sox, soy;
    GpGraphics *g = NULL;

    if (!e || cx <= 0 || cy <= 0 || e->w <= 0 || e->h <= 0) return;
    sx = (double)cx / (double)e->w;
    sy = (double)cy / (double)e->h;
    sc = (sx > sy) ? sx : sy;                  /* 取大：先保证铺满 */
    dw = (int)(e->w * sc + 0.5);
    dh = (int)(e->h * sc + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    /* 反算出可见源矩形，直接只绘制该部分，不需要剪辑区 */
    sw = (int)((double)cx / sc + 0.5);
    sh = (int)((double)cy / sc + 0.5);
    sox = (e->w - sw) / 2;
    soy = (e->h - sh) / 2;
    if (sw > e->w) { sw = e->w; sox = 0; }
    if (sh > e->h) { sh = e->h; soy = 0; }
    if (GdipCreateFromHDC(hdc, &g) != 0 || !g) return;
    GdipSetInterpolationMode(g, 6 /* HighQualityBicubic */);
    GdipDrawImageRectRectI(g, e->img, x, y, cx, cy, sox, soy, sw, sh,
                           2 /* UnitPixel */, NULL, NULL, NULL);
    GdipDeleteGraphics(g);
}

/* 等比缩放“完整显示”在框里（contain）：允许放大，但绝不裁剪、不变形。
   框的比例跟图片一致时正好铺满，不会留下无效空白。 */
void img_draw_fit(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy)
{
    FullEnt *e = img_get_full(path);    /* 用原尺寸，放大也清晰 */
    double sx, sy, sc;
    int dw, dh, dx, dy;
    GpGraphics *g = NULL;

    if (!e || cx <= 0 || cy <= 0 || e->w <= 0 || e->h <= 0) return;
    sx = (double)cx / (double)e->w;
    sy = (double)cy / (double)e->h;
    sc = (sx < sy) ? sx : sy;                  /* 取小：整张都放得下 */
    dw = (int)(e->w * sc + 0.5);
    dh = (int)(e->h * sc + 0.5);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    dx = x + (cx - dw) / 2;
    dy = y + (cy - dh) / 2;
    if (GdipCreateFromHDC(hdc, &g) != 0 || !g) return;
    GdipSetInterpolationMode(g, 6 /* HighQualityBicubic */);
    GdipDrawImageRectI(g, e->img, dx, dy, dw, dh);
    GdipDeleteGraphics(g);
}

/* ==========================================================================
 * 便笺上的图片框：位置与尺寸都可自定义（千分比），
 * 且始终保证图片下方给正文留出 minTextH，不会把文字挤掉。
 * ========================================================================*/
int img_box_in(const RECT *content, const Schedule *s, int minTextH, int minImgH,
               int minImgW, RECT *out)
{
    int cw, ch, availH, x, y, w, h;

    if (!content || !s || !out) return 0;
    cw = content->right - content->left;
    ch = content->bottom - content->top;
    if (cw < 24 || ch < 24) return 0;
    if (minTextH > ch - minImgH) minTextH = ch - minImgH;   /* 空间太小就先保证图片 */
    if (minTextH < 0) minTextH = 0;
    availH = ch - minTextH;
    if (availH < minImgH) return 0;

    if (s->imgW > 0 && s->imgH > 0) {          /* 用户自己摆的位置与尺寸 */
        w = (int)((long long)cw * s->imgW / 1000);
        h = (int)((long long)ch * s->imgH / 1000);
        x = content->left + (int)((long long)cw * s->imgX / 1000);
        y = content->top + (int)((long long)ch * s->imgY / 1000);
    } else {                                    /* 自动：横向铺满，高度按百分比 */
        int pct = s->imgPct ? s->imgPct : 55;
        w = cw;
        x = content->left;
        y = content->top;
        h = (int)((long long)availH * pct / 100);
    }
    if (minImgW < 8) minImgW = 8;
    if (w < minImgW) w = minImgW;
    if (h < minImgH) h = minImgH;
    if (w > cw) w = cw;
    if (h > availH) h = availH;
    if (x < content->left) x = content->left;
    if (x + w > content->right) x = content->right - w;
    if (y < content->top) y = content->top;
    if (y + h > content->top + availH) y = content->top + availH - h;
    if (y < content->top) { y = content->top; h = availH; }
    out->left = x;
    out->top = y;
    out->right = x + w;
    out->bottom = y + h;
    return 1;
}

/* 把图片框换算回千分比存起来 */
void img_box_store(const RECT *content, const RECT *box, Schedule *s)
{
    int cw, ch;
    if (!content || !box || !s) return;
    cw = content->right - content->left;
    ch = content->bottom - content->top;
    if (cw < 8 || ch < 8) return;
    s->imgX = (box->left - content->left) * 1000 / cw;
    s->imgY = (box->top - content->top) * 1000 / ch;
    s->imgW = (box->right - box->left) * 1000 / cw;
    s->imgH = (box->bottom - box->top) * 1000 / ch;
    if (s->imgX < 0) s->imgX = 0;
    if (s->imgY < 0) s->imgY = 0;
    if (s->imgX > 1000) s->imgX = 1000;
    if (s->imgY > 1000) s->imgY = 1000;
    if (s->imgW < 60) s->imgW = 60;
    if (s->imgH < 60) s->imgH = 60;
    if (s->imgW > 1000) s->imgW = 1000;
    if (s->imgH > 1000) s->imgH = 1000;
}

/* 命中图片框的手柄（四角 + 四边中点） */
int img_handle_at(const RECT *box, int x, int y, int hdl)
{
    int g = hdl > 0 ? hdl : 10;
    int nearL, nearR, nearT, nearB, inside;
    if (!box) return IMG_HDL_NONE;
    nearL = (x >= box->left - g && x <= box->left + g);
    nearR = (x >= box->right - g && x <= box->right + g);
    nearT = (y >= box->top - g && y <= box->top + g);
    nearB = (y >= box->bottom - g && y <= box->bottom + g);
    inside = (x >= box->left - g && x <= box->right + g &&
              y >= box->top - g && y <= box->bottom + g);
    if (!inside) return IMG_HDL_NONE;
    {
        int m = IMG_HDL_NONE;
        if (nearL) m |= IMG_HDL_L;
        if (nearR) m |= IMG_HDL_R;
        if (nearT) m |= IMG_HDL_T;
        if (nearB) m |= IMG_HDL_B;
        return m;
    }
}


/* 按原分辨率cover（便笺上的照片要清晰） */

/* 兼容入口：与 img_draw_cover 同义（后者已经用原分辨率） */
void img_draw_cover_full(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy)
{
    img_draw_cover(hdc, path, x, y, cx, cy);
}

/* 非图片文件：画一个带扩展名的文件小图标 */
void img_draw_file_icon(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy)
{
    RECT r;
    HBRUSH br;
    HPEN pen, oldPen;
    HFONT oldFont;
    COLORREF bg = RGB(228, 233, 240), fg = RGB(88, 100, 120);
    wchar_t ext[12];
    const wchar_t *dot = NULL, *p;
    int i = 0;

    for (p = path; *p; p++) {
        if (*p == L'.') dot = p;
        else if (*p == L'\\' || *p == L'/') dot = NULL;
    }
    if (dot && dot[1]) {
        for (p = dot + 1; *p && i < 8; p++) {
            wchar_t c = *p;
            if (c >= L'a' && c <= L'z') c -= 32;
            ext[i++] = c;
        }
    }
    ext[i] = 0;
    if (!ext[0]) wcscpy(ext, L"FILE");

    r.left = x; r.top = y; r.right = x + cx; r.bottom = y + cy;
    br = CreateSolidBrush(bg);
    pen = CreatePen(PS_SOLID, 1, RGB(200, 208, 220));
    oldPen = (HPEN)SelectObject(hdc, pen);
    {
        HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, 8, 8);
        SelectObject(hdc, ob);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    DeleteObject(br);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    oldFont = (HFONT)SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    r.top += (cy - 14) / 2;
    DrawTextW(hdc, ext, -1, &r, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}
