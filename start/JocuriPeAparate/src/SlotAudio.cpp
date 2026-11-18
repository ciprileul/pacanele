#include "SlotAudio.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#if defined(_MSC_VER)
#pragma comment(lib, "winmm.lib")   // cu MinGW adauga -lwinmm la linker
#endif
#endif

namespace slot {
namespace audio {
namespace {

const int RATE = 44100;
const char* const FILES[SND_TOTAL] = {"click", "spin", "reelstop", "scatter", "anticip", "win1", "win2", "bigwin",
                                      "coins", "count", "fsintro", "expand", "fsend", "card", "gwin", "glose",
                                      "jackpot", "buzz", "page"};

struct Voice {
    int id, sound, owner;
    size_t pos;
    float vol;
    bool loop;
    float fade;      // 1 = normal; < 1 = se stinge
    bool stopping;
};

std::vector<float> gSamples[SND_TOTAL];
std::vector<Voice> gVoices;
std::vector<float> gOwnerVol(1, 1.0f);
std::mutex gMutex;
int gNextVoice = 1;
float gMaster = 0.8f;
bool gMuted = false;
bool gLoaded = false;
int gPlayCount[SND_TOTAL] = {0};

uint32_t rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool loadWav(const std::string& path, std::vector<float>& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < 12 || std::memcmp(&b[0], "RIFF", 4) || std::memcmp(&b[8], "WAVE", 4)) return false;
    int channels = 0, rate = 0, bits = 0, format = 0;
    size_t p = 12;
    const unsigned char* data = nullptr;
    size_t dataLen = 0;
    while (p + 8 <= b.size()) {
        uint32_t len = rd32(&b[p + 4]);
        const unsigned char* body = &b[p + 8];
        if (p + 8 + len > b.size()) len = (uint32_t)(b.size() - p - 8);
        if (!std::memcmp(&b[p], "fmt ", 4) && len >= 16) {
            format = rd16(body);
            channels = rd16(body + 2);
            rate = (int)rd32(body + 4);
            bits = rd16(body + 14);
        } else if (!std::memcmp(&b[p], "data", 4)) {
            data = body;
            dataLen = len;
        }
        p += 8 + len + (len & 1);
    }
    if (!data || format != 1 || bits != 16 || channels < 1 || channels > 2 || rate <= 0) return false;
    size_t frames = dataLen / (2 * (size_t)channels);
    std::vector<float> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        float s = 0;
        for (int c = 0; c < channels; ++c) s += (int16_t)rd16(data + (i * channels + c) * 2) / 32768.0f;
        mono[i] = s / channels;
    }
    if (rate == RATE) {
        out.swap(mono);
    } else {   // reesantionare liniara la 44100 Hz
        size_t n = (size_t)((double)frames * RATE / rate);
        out.resize(n);
        for (size_t i = 0; i < n; ++i) {
            double src = (double)i * rate / RATE;
            size_t a = (size_t)src;
            double t = src - a;
            float x0 = mono[a < frames ? a : frames - 1], x1 = mono[a + 1 < frames ? a + 1 : frames - 1];
            out[i] = (float)(x0 + (x1 - x0) * t);
        }
    }
    return true;
}

#if defined(_WIN32)
const int NBUF = 4;
const int FRAMES = 512;
HWAVEOUT gDev = nullptr;
WAVEHDR gHdr[NBUF];
short gBuf[NBUF][FRAMES];
bool gQueued[NBUF];
std::thread gThread;
std::atomic<bool> gRun(false);

void audioThread() {
    std::vector<float> tmp(FRAMES);
    while (gRun.load()) {
        for (int i = 0; i < NBUF; ++i) {
            if (gQueued[i] && !(gHdr[i].dwFlags & WHDR_DONE)) continue;
            mix(tmp.data(), FRAMES);
            for (int k = 0; k < FRAMES; ++k) {
                float v = tmp[k];
                v = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
                gBuf[i][k] = (short)(v * 32767.0f);
            }
            gHdr[i].dwFlags &= ~WHDR_DONE;
            waveOutWrite(gDev, &gHdr[i], sizeof(WAVEHDR));
            gQueued[i] = true;
        }
        Sleep(2);
    }
}

bool openDevice() {
    WAVEFORMATEX wf;
    std::memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 1;
    wf.nSamplesPerSec = RATE;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2;
    wf.nAvgBytesPerSec = RATE * 2;
    if (waveOutOpen(&gDev, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        gDev = nullptr;
        return false;
    }
    for (int i = 0; i < NBUF; ++i) {
        std::memset(&gHdr[i], 0, sizeof(WAVEHDR));
        gHdr[i].lpData = reinterpret_cast<LPSTR>(gBuf[i]);
        gHdr[i].dwBufferLength = FRAMES * sizeof(short);
        waveOutPrepareHeader(gDev, &gHdr[i], sizeof(WAVEHDR));
        gQueued[i] = false;
    }
    gRun = true;
    gThread = std::thread(audioThread);
    return true;
}

void closeDevice() {
    if (!gDev) return;
    gRun = false;
    if (gThread.joinable()) gThread.join();
    waveOutReset(gDev);
    for (int i = 0; i < NBUF; ++i) waveOutUnprepareHeader(gDev, &gHdr[i], sizeof(WAVEHDR));
    waveOutClose(gDev);
    gDev = nullptr;
}
bool deviceOpen() { return gDev != nullptr; }
// daca programul se inchide fara shutdown(), oprim firul audio inainte sa fie distrus
struct ExitGuard {
    ~ExitGuard() { closeDevice(); }
} gExitGuard;
#else
bool openDevice() { return false; }
void closeDevice() {}
bool deviceOpen() { return false; }
#endif

} // namespace

bool init(const std::string& soundDir) {
    if (gLoaded) return deviceOpen();
    std::string dir = soundDir;
    if (!dir.empty() && dir[dir.size() - 1] != '/' && dir[dir.size() - 1] != '\\') dir += "/";
    int ok = 0;
    for (int s = 0; s < SND_TOTAL; ++s) {
        if (loadWav(dir + FILES[s] + ".wav", gSamples[s])) ++ok;
        else std::fprintf(stderr, "[SlotAudio] lipseste %s%s.wav\n", dir.c_str(), FILES[s]);
    }
    gLoaded = true;
    if (ok == 0) return false;
    bool dev = openDevice();
    if (!dev) std::fprintf(stderr, "[SlotAudio] nu exista dispozitiv audio - aparatele vor fi silentioase\n");
    return dev;
}

void shutdown() {
    closeDevice();
    std::lock_guard<std::mutex> lk(gMutex);
    gVoices.clear();
    for (int s = 0; s < SND_TOTAL; ++s) std::vector<float>().swap(gSamples[s]);
    gLoaded = false;
}

bool available() { return deviceOpen(); }

int newOwner() {
    std::lock_guard<std::mutex> lk(gMutex);
    gOwnerVol.push_back(1.0f);
    return (int)gOwnerVol.size() - 1;
}

int play(int sound, int owner, float volume, bool loop) {
    if (sound < 0 || sound >= SND_TOTAL) return 0;
    std::lock_guard<std::mutex> lk(gMutex);
    ++gPlayCount[sound];
    if (gSamples[sound].empty() || gVoices.size() >= 48) return 0;
    Voice v;
    v.id = gNextVoice++;
    v.sound = sound;
    v.owner = owner;
    v.pos = 0;
    v.vol = volume;
    v.loop = loop;
    v.fade = 1.0f;
    v.stopping = false;
    gVoices.push_back(v);
    return v.id;
}

void stop(int voice) {
    if (voice <= 0) return;
    std::lock_guard<std::mutex> lk(gMutex);
    for (size_t i = 0; i < gVoices.size(); ++i)
        if (gVoices[i].id == voice) gVoices[i].stopping = true;
}

void stopOwner(int owner) {
    std::lock_guard<std::mutex> lk(gMutex);
    for (size_t i = 0; i < gVoices.size(); ++i)
        if (gVoices[i].owner == owner) gVoices[i].stopping = true;
}

void setOwnerVolume(int owner, float v) {
    std::lock_guard<std::mutex> lk(gMutex);
    if (owner >= 0 && owner < (int)gOwnerVol.size()) gOwnerVol[(size_t)owner] = v < 0 ? 0 : (v > 1 ? 1 : v);
}

void setMasterVolume(float v) {
    std::lock_guard<std::mutex> lk(gMutex);
    gMaster = v < 0 ? 0 : (v > 1 ? 1 : v);
}
float masterVolume() { return gMaster; }
void setMuted(bool m) {
    std::lock_guard<std::mutex> lk(gMutex);
    gMuted = m;
}
bool muted() { return gMuted; }
int playCount(int sound) { return (sound >= 0 && sound < SND_TOTAL) ? gPlayCount[sound] : 0; }

void mix(float* out, int frames) {
    for (int i = 0; i < frames; ++i) out[i] = 0.0f;
    std::lock_guard<std::mutex> lk(gMutex);
    const float fadeStep = 1.0f / (0.012f * RATE);   // ~12 ms
    for (size_t vi = 0; vi < gVoices.size();) {
        Voice& v = gVoices[vi];
        const std::vector<float>& s = gSamples[v.sound];
        float ov = (v.owner >= 0 && v.owner < (int)gOwnerVol.size()) ? gOwnerVol[(size_t)v.owner] : 1.0f;
        float g = gMuted ? 0.0f : v.vol * ov * gMaster;
        bool dead = s.empty();
        for (int i = 0; i < frames && !dead; ++i) {
            if (v.stopping) {
                v.fade -= fadeStep;
                if (v.fade <= 0) {
                    dead = true;
                    break;
                }
            }
            out[i] += s[v.pos] * g * v.fade;
            if (++v.pos >= s.size()) {
                if (v.loop) v.pos = 0;
                else dead = true;
            }
        }
        if (dead) {
            gVoices[vi] = gVoices.back();
            gVoices.pop_back();
        } else {
            ++vi;
        }
    }
    for (int i = 0; i < frames; ++i) {   // limitator moale peste 0.8 (fara distorsiuni la volum normal)
        float x = out[i], a = std::fabs(x);
        if (a > 0.8f) out[i] = (x < 0 ? -1.0f : 1.0f) * (0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f));
    }
}

} // namespace audio
} // namespace slot
