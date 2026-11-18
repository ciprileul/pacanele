#pragma once
// ---------------------------------------------------------------------------
//  SlotAudio - mixer de sunet minimal pentru aparate.
//  Pe Windows reda prin waveOut (winmm, inclus in Windows) pe un fir separat;
//  mai multe sunete pot suna simultan. Pe alte sisteme functiile exista,
//  dar nu se aude nimic (mix() poate fi folosit cu alt backend).
//  Sunetele sunt fisiere WAV din assets/sounds/ (le poti inlocui).
// ---------------------------------------------------------------------------
#include <string>

namespace slot {
namespace audio {

enum Sound {
    SND_CLICK = 0, SND_SPIN, SND_REELSTOP, SND_SCATTER, SND_ANTICIP, SND_WIN1, SND_WIN2, SND_BIGWIN,
    SND_COINS, SND_COUNT, SND_FSINTRO, SND_EXPAND, SND_FSEND, SND_CARD, SND_GWIN, SND_GLOSE,
    SND_JACKPOT, SND_BUZZ, SND_PAGE, SND_TOTAL
};

bool init(const std::string& soundDir);    // incarca sunetele si porneste dispozitivul audio
void shutdown();
bool available();                          // true daca se aude ceva (dispozitiv deschis)

int newOwner();                            // fiecare aparat are propriul "proprietar" (volum separat)
int play(int sound, int owner, float volume = 1.0f, bool loop = false);   // intoarce id-ul vocii (0 = nimic)
void stop(int voice);                      // oprire lina (fara pocnet)
void stopOwner(int owner);
void setOwnerVolume(int owner, float v);   // 0..1, de ex. dupa distanta pana la camera

void setMasterVolume(float v);
float masterVolume();
void setMuted(bool m);
bool muted();

void mix(float* out, int frames);          // amesteca vocile active (mono, 44100 Hz)
int playCount(int sound);                  // de cate ori s-a cerut un sunet (pentru teste)

} // namespace audio
} // namespace slot
