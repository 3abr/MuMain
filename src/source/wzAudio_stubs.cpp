// Stub implementations of wzAudio for platforms without the library
// This allows builds on systems where wzAudio x64 is unavailable
#include "stdafx.h"
#include "../dependencies/include/wzAudio.h"

// wzAudioCreate - Initialize audio system
int wzAudioCreate(HWND hParentWnd) {
    return 0; // Success - audio disabled
}

// wzAudioDestroy - Shutdown audio system
void wzAudioDestroy() {
    // No-op
}

// wzAudioPlay - Play a sound
void wzAudioPlay(const char* szFilename, int numRepeat) {
    // No-op - audio disabled
}

// wzAudioPause - Pause playback
void wzAudioPause() {
    // No-op
}

// wzAudioStop - Stop playback
void wzAudioStop() {
    // No-op
}

// wzAudioSetVolume - Set sound volume level
void wzAudioSetVolume(int numVolume) {
    // No-op
}

// wzAudioGetVolume - Get current sound volume level
int wzAudioGetVolume() {
    return 0; // Volume is off
}

// wzAudioVolumeUp - Increase volume
void wzAudioVolumeUp() {
    // No-op
}

// wzAudioVolumeDown - Decrease volume
void wzAudioVolumeDown() {
    // No-op
}

// wzAudioOpenFile - Open file dialog
int wzAudioOpenFile(char* szFilenameBuf) {
    return 0; // No file selected
}

// wzAudioSeek - Seek to position
void wzAudioSeek(int nPosition) {
    // No-op
}

// wzAudioGetStreamOffsetRange - Get current playback position
int wzAudioGetStreamOffsetRange() {
    return 0; // No stream
}

// wzAudioGetStreamOffsetSec - Get playback position in seconds
int wzAudioGetStreamOffsetSec() {
    return 0; // No stream
}

// wzAudioSetMixerMode - Select mixer
void wzAudioSetMixerMode(int nMixerMode) {
    // No-op
}

// wzAudioGetStreamInfo - Query bitrate and frequency
void wzAudioGetStreamInfo(char* lpszBitrate, char* lpszFrequency) {
    if (lpszBitrate) lpszBitrate[0] = '\0';
    if (lpszFrequency) lpszFrequency[0] = '\0';
}

// wzAudioOption - Set options
void wzAudioOption(int nOption, int nVal) {
    // No-op
}

// wzAudioSetEqualizer - Tune equalizer
void wzAudioSetEqualizer(const int Slider[MAX_EQ_BANKSLOTS]) {
    // No-op
}
