/* 临时探针：测试这台机器上 MCI / DirectShow / Media Foundation 对 wav/mp3/flac 的支持。
   mci:      open + status length
   dshow:    IGraphBuilder::RenderFile
   mf:       MFStartup + MFCreateSourceReaderFromURL + 是否解得出 PCM 类型
   用法：audioprobe.exe <文件> ...（结果写到 _tools\audioprobe_out.txt） */
#define STRSAFE_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <dshow.h>

static FILE *g_out;

static void P(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_out, fmt, ap);
    va_end(ap);
    fputc('\n', g_out);
}

static void try_mci(const wchar_t *path)
{
    wchar_t cmd[1024], buf[256];
    MCIERROR e;
    wcscpy(cmd, L"open \"");
    wcsncat(cmd, path, 900);
    wcscat(cmd, L"\" alias ap");
    e = mciSendStringW(cmd, NULL, 0, NULL);
    if (e) {
        wchar_t err[256];
        mciGetErrorStringW(e, err, 256);
        P("  MCI open   FAIL  err=%u [%ls]", (unsigned)e, err);
        return;
    }
    e = mciSendStringW(L"status ap length", buf, 256, NULL);
    P("  MCI open   OK    length=%ls  err=%u", (e ? L"?" : buf), (unsigned)e);
    /* 试着真的播放 200ms（不 notify，直接 stop/close） */
    e = mciSendStringW(L"play ap", NULL, 0, NULL);
    P("  MCI play   %s err=%u", e ? "FAIL" : "OK", (unsigned)e);
    Sleep(200);
    mciSendStringW(L"stop ap", NULL, 0, NULL);
    mciSendStringW(L"close ap", NULL, 0, NULL);
}

static void try_dshow(const wchar_t *path)
{
    IGraphBuilder *gb = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IGraphBuilder, (void **)&gb);
    if (FAILED(hr) || !gb) { P("  DShow      FAIL CoCreate 0x%08lx", (unsigned long)hr); return; }
    hr = gb->lpVtbl->RenderFile(gb, path, NULL);
    P("  DShow      RenderFile %s (0x%08lx)", SUCCEEDED(hr) ? "OK" : "FAIL", (unsigned long)hr);
    if (SUCCEEDED(hr)) {
        IMediaControl *mc = NULL;
        if (SUCCEEDED(gb->lpVtbl->QueryInterface(gb, &IID_IMediaControl, (void **)&mc)) && mc) {
            hr = mc->lpVtbl->Run(mc);
            P("  DShow      Run %s (0x%08lx)", SUCCEEDED(hr) ? "OK" : "FAIL", (unsigned long)hr);
            Sleep(200);
            mc->lpVtbl->Stop(mc);
            mc->lpVtbl->Release(mc);
        }
    }
    gb->lpVtbl->Release(gb);
}

static int mf_ready;
static void try_mf(const wchar_t *path)
{
    IMFSourceReader *rdr = NULL;
    IMFMediaType *mt = NULL;
    HRESULT hr;
    if (!mf_ready) { P("  MF         MFStartup FAIL"); return; }
    hr = MFCreateSourceReaderFromURL(path, NULL, &rdr);
    if (FAILED(hr) || !rdr) { P("  MF         Open FAIL 0x%08lx", (unsigned long)hr); return; }
    P("  MF         Open OK");
    /* 当前媒体类型 */
    if (SUCCEEDED(rdr->lpVtbl->GetNativeMediaType(rdr, 0, 0, &mt)) && mt) {
        GUID sub = { 0 };
        mt->lpVtbl->GetGUID(mt, &MF_MT_SUBTYPE, &sub);
        P("  MF         native subtype %08lx-%04x", (unsigned long)sub.Data1, sub.Data2);
        mt->lpVtbl->Release(mt);
        mt = NULL;
    }
    /* 要求解成 16bit PCM（不设格式 = 由 MF 自动选） */
    hr = MFCreateMediaType(&mt);
    if (SUCCEEDED(hr) && mt) {
        mt->lpVtbl->SetGUID(mt, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
        mt->lpVtbl->SetGUID(mt, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
        hr = rdr->lpVtbl->SetCurrentMediaType(rdr, 0, NULL, mt);
        P("  MF         -> PCM %s (0x%08lx)", SUCCEEDED(hr) ? "OK" : "FAIL", (unsigned long)hr);
        mt->lpVtbl->Release(mt);
    }
    if (SUCCEEDED(hr)) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        IMFSample *smp = NULL;
        int got = 0;
        while (got < 3) {
            HRESULT r = rdr->lpVtbl->ReadSample(rdr, 0, 0, NULL, &flags, &ts, &smp);
            if (FAILED(r)) { P("  MF         ReadSample FAIL 0x%08lx", (unsigned long)r); break; }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { P("  MF         EOS after %d samples", got); break; }
            if (smp) { smp->lpVtbl->Release(smp); smp = NULL; got++; }
        }
        if (got) P("  MF         ReadSample OK (%d)", got);
    }
    rdr->lpVtbl->Release(rdr);
}

int main(int argc, char **argv)
{
    int i, mfhr;
    g_out = fopen("_tools\\audioprobe_out.txt", "wb");
    if (!g_out) return 1;
    fputs("\xEF\xBB\xBF", g_out);
    mfhr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    mf_ready = SUCCEEDED(mfhr);
    P("MFStartup hr=0x%08lx", (unsigned long)mfhr);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    for (i = 1; i < argc; i++) {
        wchar_t wpath[MAX_PATH];
        char u8[MAX_PATH * 3];
        MultiByteToWideChar(CP_UTF8, 0, argv[i], -1, wpath, MAX_PATH);
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, u8, sizeof(u8), NULL, NULL);
        P("");
        P("== %s", u8);
        try_mci(wpath);
        try_dshow(wpath);
        try_mf(wpath);
    }
    CoUninitialize();
    if (mf_ready) MFShutdown();
    fclose(g_out);
    return 0;
}
