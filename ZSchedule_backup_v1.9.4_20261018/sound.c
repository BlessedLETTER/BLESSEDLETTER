/* ============================================================================
 *  ZSchedule - sound.c
 *  自选铃声的播放：
 *    ① Media Foundation（SourceResolver + 音频渲染拓扑）——wav / mp3 / flac /
 *       m4a / wma / aac 都能直接播；Windows 10+ 自带 FLAC 解码器。
 *    ② MCI（winmm）——MF 不可用时兜底，能放 wav / mp3（老系统也有）。
 *    ③ 都不行 → 交给系统默认程序打开（v1.5 的老办法），保证“总有声音”。
 *
 *  设计取舍：铃声要短平快，不需要进度条/暂停，所以没有实现事件回调；
 *  播放在 MF 自己的工作队列线程里跑，UI 线程不会被卡住。
 * ==========================================================================*/
#include "zsched.h"
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

static int              g_mfState;      /* 0 = 没试过, 1 = 可用, -1 = 不可用 */
static IMFMediaSession *g_session;
static IMFTopology     *g_topo;
static int              g_mciOpen;
static int              g_active;       /* 我们启动过一个声音（还没 stop） */
static ULONGLONG        g_startTick;    /* 开始播放的时刻（GetTickCount64） */
static LONGLONG         g_durMs;        /* 时长（毫秒），-1 = 不知道 */

/* ------------------------------------------------------------------ MF */
static int mf_ready(void)
{
    if (!g_mfState) {
        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
        g_mfState = SUCCEEDED(hr) ? 1 : -1;
    }
    return g_mfState > 0;
}

static void mf_stop(void)
{
    if (g_session) {
        g_session->lpVtbl->Stop(g_session);
        g_session->lpVtbl->Shutdown(g_session);
        g_session->lpVtbl->Release(g_session);
        g_session = NULL;
    }
    if (g_topo) {
        g_topo->lpVtbl->Release(g_topo);
        g_topo = NULL;
    }
}

static int mf_play(const wchar_t *path)
{
    IMFSourceResolver *res = NULL;
    IUnknown *unk = NULL;
    MF_OBJECT_TYPE otype = MF_OBJECT_INVALID;
    IMFMediaSource *src = NULL;
    IMFPresentationDescriptor *pd = NULL;
    IMFStreamDescriptor *sd = NULL;
    IMFTopology *topo = NULL;
    IMFTopologyNode *srcNode = NULL, *outNode = NULL;
    IMFActivate *act = NULL;
    IMFMediaSession *ses = NULL;
    BOOL selected = FALSE;
    HRESULT hr;

    if (!mf_ready()) return 0;
    mf_stop();

    hr = MFCreateSourceResolver(&res);
    if (FAILED(hr) || !res) goto fail;
    hr = res->lpVtbl->CreateObjectFromURL(res, path, MF_RESOLUTION_MEDIASOURCE, NULL,
                                         &otype, &unk);
    if (FAILED(hr) || !unk) goto fail;
    hr = unk->lpVtbl->QueryInterface(unk, &IID_IMFMediaSource, (void **)&src);
    if (FAILED(hr) || !src) goto fail;
    hr = src->lpVtbl->CreatePresentationDescriptor(src, &pd);
    if (FAILED(hr) || !pd) goto fail;
    {   /* 记下时长，方便判断“放完了没有”（只用于停止按钮/小窗自动关闭） */
        UINT64 dur = 0;
        if (SUCCEEDED(pd->lpVtbl->GetUINT64(pd, &MF_PD_DURATION, &dur)) && dur > 0)
            g_durMs = (LONGLONG)(dur / 10000);
        else
            g_durMs = -1;
    }
    hr = pd->lpVtbl->GetStreamDescriptorByIndex(pd, 0, &selected, &sd);
    if (FAILED(hr) || !selected || !sd) goto fail;

    hr = MFCreateTopology(&topo);
    if (FAILED(hr) || !topo) goto fail;
    hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &srcNode);
    if (FAILED(hr) || !srcNode) goto fail;
    srcNode->lpVtbl->SetUnknown(srcNode, &MF_TOPONODE_SOURCE, (IUnknown *)src);
    srcNode->lpVtbl->SetUnknown(srcNode, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
    srcNode->lpVtbl->SetUnknown(srcNode, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)sd);
    hr = MFCreateAudioRendererActivate(&act);
    if (FAILED(hr) || !act) goto fail;
    hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &outNode);
    if (FAILED(hr) || !outNode) goto fail;
    outNode->lpVtbl->SetObject(outNode, (IUnknown *)act);
    topo->lpVtbl->AddNode(topo, srcNode);
    topo->lpVtbl->AddNode(topo, outNode);
    hr = srcNode->lpVtbl->ConnectOutput(srcNode, 0, outNode, 0);
    if (FAILED(hr)) goto fail;

    hr = MFCreateMediaSession(NULL, &ses);
    if (FAILED(hr) || !ses) goto fail;
    hr = ses->lpVtbl->SetTopology(ses, 0, topo);
    if (FAILED(hr)) goto fail;
    {
        PROPVARIANT var;                 /* VT_EMPTY = 从头播；时间格式 NULL = 默认 */
        PropVariantInit(&var);
        hr = ses->lpVtbl->Start(ses, NULL, &var);
    }
    if (FAILED(hr)) goto fail;

    /* 成功：会话与拓扑都留着，Stop 的时候再放 */
    g_session = ses;
    g_topo = topo;
    if (srcNode) srcNode->lpVtbl->Release(srcNode);
    if (outNode) outNode->lpVtbl->Release(outNode);
    if (act) act->lpVtbl->Release(act);
    if (src) src->lpVtbl->Release(src);
    if (pd) pd->lpVtbl->Release(pd);
    if (sd) sd->lpVtbl->Release(sd);
    if (unk) unk->lpVtbl->Release(unk);
    if (res) res->lpVtbl->Release(res);
    return 1;

fail:
    if (ses) ses->lpVtbl->Release(ses);
    if (topo) topo->lpVtbl->Release(topo);
    if (srcNode) srcNode->lpVtbl->Release(srcNode);
    if (outNode) outNode->lpVtbl->Release(outNode);
    if (act) act->lpVtbl->Release(act);
    if (src) src->lpVtbl->Release(src);
    if (pd) pd->lpVtbl->Release(pd);
    if (sd) sd->lpVtbl->Release(sd);
    if (unk) unk->lpVtbl->Release(unk);
    if (res) res->lpVtbl->Release(res);
    return 0;
}

/* ------------------------------------------------------------------ MCI */
static void mci_stop(void)
{
    if (!g_mciOpen) return;
    mciSendStringW(L"stop zsring", NULL, 0, NULL);
    mciSendStringW(L"close zsring", NULL, 0, NULL);
    g_mciOpen = 0;
}

static int mci_play(const wchar_t *path)
{
    wchar_t cmd[MAX_PATH + 64];
    MCIERROR e;
    mci_stop();
    g_durMs = -1;                    /* MCI 这条路不知道时长，用 status 判断 */
    wcscpy(cmd, L"open \"");
    wcsncat(cmd, path, MAX_PATH - 1);
    wcscat(cmd, L"\" alias zsring");
    e = mciSendStringW(cmd, NULL, 0, NULL);
    if (e) return 0;
    g_mciOpen = 1;
    e = mciSendStringW(L"play zsring", NULL, 0, NULL);
    if (e) { mci_stop(); return 0; }
    return 1;
}

/* ------------------------------------------------------------------ 对外 */
void snd_stop(void)
{
    mf_stop();
    mci_stop();
    g_active = 0;
    g_durMs = -1;
}

int snd_is_active(void)
{
    if (!g_active) return 0;
    if (g_mciOpen) {                 /* MCI 能直接问“还在播吗” */
        wchar_t mode[32];
        mode[0] = 0;
        if (!mciSendStringW(L"status zsring mode", mode, 32, NULL) &&
            mode[0] && _wcsicmp(mode, L"stopped") != 0)
            return 1;
        snd_stop();
        return 0;
    }
    if (g_durMs > 0) {               /* MF 按已知时长判断播完没有 */
        if (GetTickCount64() - g_startTick > (ULONGLONG)g_durMs + 700) {
            snd_stop();
            return 0;
        }
    }
    return 1;
}

/* 播放一个声音文件：返回实际走的通道（0 = 全失败）
 *   1 = Media Foundation   2 = MCI   3 = 交给系统默认程序 */
int snd_play_file(const wchar_t *path)
{
    snd_stop();
    if (!path || !path[0]) return 0;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return 0;
    g_durMs = -1;
    if (mf_play(path)) {
        g_active = 1;
        g_startTick = GetTickCount64();
        return 1;
    }
    if (mci_play(path)) {
        g_active = 1;
        g_startTick = GetTickCount64();
        return 2;
    }
    /* 兜底：交给系统默认程序（v1.5 的老行为）——声音总会响，只是会拉起播放器 */
    if ((INT_PTR)ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL) > 32) {
        g_active = 1;
        g_durMs = -1;
        g_startTick = GetTickCount64();
        return 3;
    }
    return 0;
}

void snd_shutdown(void)
{
    snd_stop();
    if (g_mfState > 0) {
        MFShutdown();
        g_mfState = -1;
    }
}
