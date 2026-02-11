#include "stdafx.h"
#include "../dependencies/include/wzAudio.h"

#include <dsound.h>
#include <windows.h>
#include <commdlg.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>

#define STB_VORBIS_HEADER_ONLY
#include "../ThirdParty/stb/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

#pragma comment(lib, "dsound")
#pragma comment(lib, "dxguid")

static LPDIRECTSOUND8       g_pDS = nullptr;
static LPDIRECTSOUNDBUFFER  g_pDSBuffer = nullptr;
static stb_vorbis*          g_pVorbis = nullptr;
static HANDLE               g_hPlayThread = nullptr;
static volatile bool        g_bPlaying = false;
static volatile bool        g_bPaused = false;
static volatile bool        g_bStopRequest = false;
static int                  g_nVolume = 100;
static int                  g_nRepeat = 1;
static int                  g_nMixerMode = 0;
static int                  g_nOptionStopBefore = 0;
static int                  g_nTotalSamples = 0;
static int                  g_nSampleRate = 0;
static int                  g_nChannels = 0;
static int                  g_nBitrate = 0;
static CRITICAL_SECTION     g_cs;
static bool                 g_bInitialized = false;

static int VolumeToDS(int vol) {
    if (vol <= 0) return DSBVOLUME_MIN;
    if (vol >= 100) return DSBVOLUME_MAX;
    return (int)(2000.0 * log10((double)vol / 100.0));
}

static void CleanupBuffer() {
    if (g_pDSBuffer) {
        g_pDSBuffer->Stop();
        g_pDSBuffer->Release();
        g_pDSBuffer = nullptr;
    }
    if (g_pVorbis) {
        stb_vorbis_close(g_pVorbis);
        g_pVorbis = nullptr;
    }
}

static DWORD WINAPI PlaybackThread(LPVOID) {
    const int BUFFER_SIZE = 4096 * 4;
    short samples[BUFFER_SIZE / 2];

    while (!g_bStopRequest) {
        if (g_bPaused) {
            Sleep(10);
            continue;
        }

        EnterCriticalSection(&g_cs);
        if (!g_pVorbis || !g_pDSBuffer) {
            LeaveCriticalSection(&g_cs);
            break;
        }

        int n = stb_vorbis_get_samples_short_interleaved(
            g_pVorbis, g_nChannels, samples, BUFFER_SIZE / 2);
        LeaveCriticalSection(&g_cs);

        if (n == 0) {
            if (g_nRepeat == -1) {
                EnterCriticalSection(&g_cs);
                if (g_pVorbis) stb_vorbis_seek_start(g_pVorbis);
                LeaveCriticalSection(&g_cs);
                continue;
            }
            if (g_nRepeat > 1) {
                g_nRepeat--;
                EnterCriticalSection(&g_cs);
                if (g_pVorbis) stb_vorbis_seek_start(g_pVorbis);
                LeaveCriticalSection(&g_cs);
                continue;
            }
            break;
        }

        int bytesWritten = n * g_nChannels * sizeof(short);
        void* ptr1 = nullptr;
        void* ptr2 = nullptr;
        DWORD size1 = 0, size2 = 0;

        EnterCriticalSection(&g_cs);
        if (!g_pDSBuffer) {
            LeaveCriticalSection(&g_cs);
            break;
        }

        DWORD status = 0;
        g_pDSBuffer->GetStatus(&status);
        if (status & DSBSTATUS_BUFFERLOST) {
            g_pDSBuffer->Restore();
        }

        HRESULT hr = g_pDSBuffer->Lock(0, bytesWritten, &ptr1, &size1, &ptr2, &size2, DSBLOCK_FROMWRITECURSOR);
        if (SUCCEEDED(hr)) {
            if (ptr1 && size1 > 0) {
                memcpy(ptr1, samples, std::min((DWORD)bytesWritten, size1));
            }
            if (ptr2 && size2 > 0 && (DWORD)bytesWritten > size1) {
                memcpy(ptr2, ((char*)samples) + size1, std::min((DWORD)bytesWritten - size1, size2));
            }
            g_pDSBuffer->Unlock(ptr1, size1, ptr2, size2);
        }

        if (!(status & DSBSTATUS_PLAYING)) {
            g_pDSBuffer->Play(0, 0, DSBPLAY_LOOPING);
        }
        LeaveCriticalSection(&g_cs);

        Sleep(5);
    }

    g_bPlaying = false;
    return 0;
}

int wzAudioCreate(HWND hParentWnd) {
    if (g_bInitialized) return 0;

    InitializeCriticalSection(&g_cs);

    HRESULT hr = DirectSoundCreate8(nullptr, &g_pDS, nullptr);
    if (FAILED(hr)) return -1;

    hr = g_pDS->SetCooperativeLevel(hParentWnd, DSSCL_PRIORITY);
    if (FAILED(hr)) {
        g_pDS->Release();
        g_pDS = nullptr;
        return -1;
    }

    g_bInitialized = true;
    return 0;
}

void wzAudioDestroy() {
    wzAudioStop();

    if (g_pDS) {
        g_pDS->Release();
        g_pDS = nullptr;
    }

    if (g_bInitialized) {
        DeleteCriticalSection(&g_cs);
        g_bInitialized = false;
    }
}

void wzAudioPlay(const char* szFilename, int numRepeat) {
    if (!g_bInitialized || !g_pDS || !szFilename) return;

    if (g_nOptionStopBefore) {
        wzAudioStop();
    }

    EnterCriticalSection(&g_cs);
    CleanupBuffer();

    int error = 0;
    g_pVorbis = stb_vorbis_open_filename(szFilename, &error, nullptr);
    if (!g_pVorbis) {
        LeaveCriticalSection(&g_cs);
        return;
    }

    stb_vorbis_info info = stb_vorbis_get_info(g_pVorbis);
    g_nSampleRate = info.sample_rate;
    g_nChannels = info.channels;
    g_nTotalSamples = stb_vorbis_stream_length_in_samples(g_pVorbis);
    g_nBitrate = (int)(stb_vorbis_stream_length_in_seconds(g_pVorbis) > 0 ?
        (stb_vorbis_stream_length_in_samples(g_pVorbis) * info.channels * 16.0 /
         stb_vorbis_stream_length_in_seconds(g_pVorbis) / 1000.0) : 0);
    g_nRepeat = numRepeat;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)info.channels;
    wfx.nSamplesPerSec = info.sample_rate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(DSBUFFERDESC);
    desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
    desc.dwBufferBytes = wfx.nAvgBytesPerSec;
    desc.lpwfxFormat = &wfx;

    HRESULT hr = g_pDS->CreateSoundBuffer(&desc, &g_pDSBuffer, nullptr);
    if (FAILED(hr)) {
        stb_vorbis_close(g_pVorbis);
        g_pVorbis = nullptr;
        LeaveCriticalSection(&g_cs);
        return;
    }

    g_pDSBuffer->SetVolume(VolumeToDS(g_nVolume));
    LeaveCriticalSection(&g_cs);

    g_bStopRequest = false;
    g_bPaused = false;
    g_bPlaying = true;
    g_hPlayThread = CreateThread(nullptr, 0, PlaybackThread, nullptr, 0, nullptr);
}

void wzAudioPause() {
    g_bPaused = true;
    EnterCriticalSection(&g_cs);
    if (g_pDSBuffer) g_pDSBuffer->Stop();
    LeaveCriticalSection(&g_cs);
}

void wzAudioStop() {
    g_bStopRequest = true;

    if (g_hPlayThread) {
        WaitForSingleObject(g_hPlayThread, 2000);
        CloseHandle(g_hPlayThread);
        g_hPlayThread = nullptr;
    }

    EnterCriticalSection(&g_cs);
    CleanupBuffer();
    LeaveCriticalSection(&g_cs);

    g_bPlaying = false;
    g_bPaused = false;
    g_bStopRequest = false;
}

void wzAudioSetVolume(int numVolume) {
    if (numVolume < 0) numVolume = 0;
    if (numVolume > 100) numVolume = 100;
    g_nVolume = numVolume;

    EnterCriticalSection(&g_cs);
    if (g_pDSBuffer) {
        g_pDSBuffer->SetVolume(VolumeToDS(g_nVolume));
    }
    LeaveCriticalSection(&g_cs);
}

int wzAudioGetVolume() {
    return g_nVolume;
}

void wzAudioVolumeUp() {
    wzAudioSetVolume(g_nVolume + 5);
}

void wzAudioVolumeDown() {
    wzAudioSetVolume(g_nVolume - 5);
}

int wzAudioOpenFile(char* szFilenameBuf) {
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Audio Files (*.ogg;*.wav)\0*.ogg;*.wav\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFilenameBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    return GetOpenFileNameA(&ofn) ? 1 : 0;
}

void wzAudioSeek(int nPosition) {
    if (nPosition < 0) nPosition = 0;
    if (nPosition > 100) nPosition = 100;

    EnterCriticalSection(&g_cs);
    if (g_pVorbis && g_nTotalSamples > 0) {
        unsigned int target = (unsigned int)((double)nPosition / 100.0 * g_nTotalSamples);
        stb_vorbis_seek(g_pVorbis, target);
    }
    LeaveCriticalSection(&g_cs);
}

int wzAudioGetStreamOffsetRange() {
    EnterCriticalSection(&g_cs);
    int result = 0;
    if (g_pVorbis && g_nTotalSamples > 0) {
        unsigned int current = stb_vorbis_get_sample_offset(g_pVorbis);
        result = (int)((double)current / (double)g_nTotalSamples * 100.0);
    } else if (!g_bPlaying && g_nTotalSamples > 0) {
        result = 100;
    }
    LeaveCriticalSection(&g_cs);
    return result;
}

int wzAudioGetStreamOffsetSec() {
    EnterCriticalSection(&g_cs);
    int result = 0;
    if (g_pVorbis && g_nSampleRate > 0) {
        unsigned int current = stb_vorbis_get_sample_offset(g_pVorbis);
        result = (int)(current / g_nSampleRate);
    }
    LeaveCriticalSection(&g_cs);
    return result;
}

void wzAudioSetMixerMode(int nMixerMode) {
    g_nMixerMode = nMixerMode;
}

void wzAudioGetStreamInfo(char* lpszBitrate, char* lpszFrequency) {
    if (lpszBitrate) {
        sprintf(lpszBitrate, "%d kbps", g_nBitrate);
    }
    if (lpszFrequency) {
        sprintf(lpszFrequency, "%d Hz", g_nSampleRate);
    }
}

void wzAudioOption(int nOption, int nVal) {
    switch (nOption) {
    case WZAOPT_STOPBEFOREPLAY:
        g_nOptionStopBefore = nVal;
        break;
    default:
        break;
    }
}

void wzAudioSetEqualizer(const int Slider[MAX_EQ_BANKSLOTS]) {
}
