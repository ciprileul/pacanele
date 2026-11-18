// SlotMachineUI: ecranul de joc - role, prezentarea castigurilor, panoul de comanda,
// speciale, dublaj, informatii, cumpararea specialei si jackpot-ul.
// Fisier UTF-8 (cu BOM).
#include "SlotUIImpl.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace slot {
namespace {

const uint32_t LINE_COLORS[20] = {0xff3b3b, 0x3bff5a, 0x3b8bff, 0xffe03b, 0xff3bf0, 0x3bfff0, 0xff8a3b,
                                  0xa66bff, 0xb8ff3b, 0xff3b8a, 0x3bb8ff, 0xffb83b, 0x7a5bff, 0x3bff9a,
                                  0xff7a7a, 0x7affd8, 0xd8a8ff, 0xffd8a8, 0xa8ffa8, 0xa8d8ff};
const float SPIN_SPEED = 24.0f;          // simboluri / secunda
const double GAMBLE_LIMIT = 5000.0;      // castig maxim la dublaj (lei)
const int GAMBLE_MAX_STEPS = 5;
const float PI_F = 3.14159265f;
const char* const RANKS[13] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};

std::string lei(double v) {
    return fmtMoney(v) + " LEI";
}

Color lineColor(int l) {
    return hexc(LINE_COLORS[(l < 0 ? 0 : l) % 20]);
}

int posMod(int a, int m) {
    int r = a % m;
    return r < 0 ? r + m : r;
}

float hash01(int i) {
    unsigned x = (unsigned)i * 2654435761u;
    x ^= x >> 13;
    x *= 0x5bd1e995u;
    x ^= x >> 15;
    return (x & 0xffffff) / (float)0x1000000;
}

bool isScatterKind(int k) {
    return k == KIND_SCATTER || k == KIND_BOOK;
}

} // namespace

// ================================================================== logica
Rect SlotMachineUI::Impl::reelWindow() const {
    int nr = game ? game->reels : 5, nw = game ? game->rows : 3;
    float cw = nr == 3 ? 220.0f : 176.0f;
    float ch = nw == 3 ? 148.0f : 111.0f;
    float ww = nr * cw, hh = nw * ch;
    return R(640.0f - ww * 0.5f, 84.0f + (444.0f - hh) * 0.5f, ww, hh);
}

Rect SlotMachineUI::Impl::cellRect(int reel, int row) const {
    Rect w = reelWindow();
    float cw = w.w / game->reels, ch = w.h / game->rows;
    return R(w.x + reel * cw, w.y + row * ch, cw, ch);
}

double SlotMachineUI::Impl::bet() const {
    return BET_STEPS[betStep];
}

double SlotMachineUI::Impl::lineBet() const {
    return game ? bet() / game->lines : 0.0;
}

void SlotMachineUI::Impl::enterGame(int gi) {
    gameIndex = gi;
    game = &catalog().games[(size_t)gi];
    engine.setGame(game);
    betStep = std::max(betStep, minBetStep(*game));
    for (int c = 0; c < MAX_REELS; ++c) {
        std::memset(&reels[c], 0, sizeof(ReelAnim));
        if (c < game->reels) reels[c].pos = (float)engine.randInt((int)engine.stripFor(c, false).size());
    }
    spinning = false;
    haveResult = false;
    presenting = false;
    pendingWin = 0;
    winShown = winTarget = winFrom = 0;
    winCountT = 1;
    expandT = -1;
    collectT = -1;
    fishCollected = false;
    bigWinT = -1;
    autoPlay = false;
    overlay = OV_NONE;
    fsActive = false;
    reelFs = false;
    fsLeft = fsPlayed = fsTotal = 0;
    fsWon = 0;
    special = -1;
    fsPendingAward = 0;
    fsRetriggerT = -1;
    jpTierPending = -1;
    idleT = 0;
    hlOn = false;
    activeWin = -1;
    message.clear();
}

void SlotMachineUI::Impl::leaveGame() {
    collect();
    audio::stopOwner(audioOwner);
    spinVoice = anticVoice = countVoice = 0;
    autoPlay = false;
    overlay = OV_NONE;
    transTo = 0;
    transT = 0;
}

void SlotMachineUI::Impl::collect() {
    if (pendingWin > 0) {
        wallet->balance += pendingWin;
        pendingWin = 0;
        snd(audio::SND_COINS, 0.7f);
    }
    presenting = false;
}

void SlotMachineUI::Impl::changeBet(int d) {
    if (!game || spinning || fsActive || overlay != OV_NONE || bigWinT >= 0 || expandT >= 0 || collectT >= 0 || fsPendingAward > 0) return;
    collect();
    int nb = betStep + d;
    nb = std::max(minBetStep(*game), std::min(BET_STEP_COUNT - 1, nb));
    betStep = nb;
}

void SlotMachineUI::Impl::setWinCounter(double from, double to, float dur) {
    winFrom = from;
    winTarget = to;
    winCountT = 0;
    winCountDur = std::max(0.2f, dur);
    winShown = from;
    audio::stop(countVoice);
    countVoice = (to > from && winCountDur > 0.5f) ? audio::play(audio::SND_COUNT, audioOwner, 0.8f, true) : 0;
}

void SlotMachineUI::Impl::startSpin() {
    if (!game || spinning || overlay != OV_NONE || bigWinT >= 0 || expandT >= 0 || collectT >= 0 || transT < 1.0f) return;
    if (!fsActive) {
        if (fsPendingAward > 0 || jpTierPending >= 0) return;
        collect();
        double b = bet();
        if (wallet->balance + 1e-9 < b) {
            showToast(SLOT_U8("CREDIT INSUFICIENT"));
            autoPlay = false;
            return;
        }
        wallet->balance -= b;
        if (creditShown > wallet->balance) creditShown = wallet->balance;
        int tier = jackpots().contribute(b);
        if (tier >= 0) jpTierPending = tier;
        result = engine.spin(b, false, -1);
        winShown = winTarget = 0;
        winCountT = 1;
        message = "NOROC!";
    } else {
        if (fsLeft <= 0) return;
        --fsLeft;
        ++fsPlayed;
        result = engine.spin(bet(), true, special);
        message.clear();
    }
    launchReels();
}

void SlotMachineUI::Impl::startBuy() {
    if (!game || game->buyCost <= 0) return;
    double price = bet() * game->buyCost;
    overlay = OV_NONE;
    if (wallet->balance + 1e-9 < price) {
        showToast(SLOT_U8("CREDIT INSUFICIENT"));
        return;
    }
    collect();
    wallet->balance -= price;
    if (creditShown > wallet->balance) creditShown = wallet->balance;
    result = engine.forcedScatterSpin(bet());
    winShown = winTarget = 0;
    winCountT = 1;
    buying = true;
    autoPlay = false;
    message = SLOT_U8("SPECIALA CUMPĂRATĂ - NOROC!");
    launchReels();
}

void SlotMachineUI::Impl::launchReels() {
    audio::stop(countVoice);
    audio::stop(spinVoice);
    countVoice = 0;
    spinVoice = 0;   // fara sunet de fundal cat se invart rolele (doar "tac" la oprire si scatter)
    haveResult = true;
    presenting = false;
    hlOn = false;
    fishCollected = false;
    collectT = -1;
    spinning = true;
    spinT = 0;
    idleT = 0;
    if (reelFs != result.fsStrips) {
        // trecere intre benzile de baza si cele ale rotirilor gratuite: pastram faza animatiei
        reelFs = result.fsStrips;
        for (int c = 0; c < game->reels; ++c) {
            float L = (float)engine.stripFor(c, reelFs).size();
            float fr = reels[c].pos - std::floor(reels[c].pos);
            reels[c].pos = (float)posMod((int)std::floor(reels[c].pos), (int)L) + fr;
        }
    }
    float extra = 0;
    int scat = 0;
    bool anticipate = false;
    const int sp = speedNow();
    for (size_t s = 0; s < game->symbols.size(); ++s)
        if (isScatterKind(game->symbols[s].kind)) anticipate = true;
    for (int c = 0; c < game->reels; ++c) {
        ReelAnim& a = reels[c];
        a.phase = 1;
        a.t = 0;
        a.speed = 0;
        a.target = result.stops[c];
        a.anticipation = false;
        a.fast = sp == 2;
        if (anticipate && sp == 0 && c >= 2 && scat >= game->scatterMin - 1 && scat < game->scatterMin + 2) {
            a.anticipation = true;
            extra += 1.15f;
        }
        static const float BASE[3] = {0.55f, 0.28f, 0.16f}, STEP[3] = {0.19f, 0.08f, 0.035f};
        a.stopAt = (fsActive && sp == 0 ? 0.45f : BASE[sp]) + c * STEP[sp] + extra;
        for (int r = 0; r < game->rows; ++r)
            if (isScatterKind(game->symbols[(size_t)result.raw[c][r]].kind)) ++scat;
    }
}

// "Maini rapide": a doua apasare pe START / Space opreste toate rolele aproape imediat.
void SlotMachineUI::Impl::quickStop() {
    if (!spinning) return;
    for (int c = 0; c < game->reels; ++c) {
        ReelAnim& a = reels[c];
        if (a.phase == 0 || a.phase == 4) continue;
        const int L = (int)engine.stripFor(c, reelFs).size();
        float k = std::floor((a.pos - 1.0f - a.target) / L);
        a.from = a.pos;
        a.to = a.target + k * L - 0.12f;
        a.dur = 0.09f;
        a.t = 0;
        a.speed = SPIN_SPEED;
        a.anticipation = false;
        a.fast = true;
        a.phase = 3;
    }
}

void SlotMachineUI::Impl::updateReels(float dt) {
    spinT += dt;
    bool all = true;
    bool stopSnd = false;
    for (int c = 0; c < game->reels; ++c) {
        ReelAnim& a = reels[c];
        const int L = (int)engine.stripFor(c, reelFs).size();
        switch (a.phase) {
            case 1:
                a.t += dt;
                if (a.t < 0.1f) {
                    a.pos += 2.2f * dt;
                } else {
                    a.speed = std::min(SPIN_SPEED, a.speed + SPIN_SPEED * dt / 0.16f);
                    a.pos -= a.speed * dt;
                    if (a.speed >= SPIN_SPEED) a.phase = 2;
                }
                if (spinT < a.stopAt) break;
                a.speed = std::max(a.speed, SPIN_SPEED * 0.5f);
            // fall through
            case 2:
                a.phase = 2;
                a.pos -= a.speed * dt;
                if (spinT >= a.stopAt) {
                    float minTravel = a.fast ? 1.5f : 3.0f;
                    float k = std::floor((a.pos - minTravel - a.target) / L);
                    float F = a.target + k * L;
                    a.from = a.pos;
                    a.to = F - (a.fast ? 0.12f : 0.24f);
                    a.dur = a.fast ? 0.12f : clampf(2.0f * (a.from - a.to) / std::max(1.0f, a.speed), 0.16f, speedNow() == 1 ? 0.42f : 0.9f);
                    a.t = 0;
                    a.phase = 3;
                }
                break;
            case 3: {
                a.t += dt;
                float u = std::min(1.0f, a.t / a.dur);
                a.pos = a.from + (a.to - a.from) * (1.0f - (1.0f - u) * (1.0f - u));
                a.speed = SPIN_SPEED * (1.0f - u);
                if (u >= 1.0f) {
                    a.phase = 4;
                    a.t = 0;
                    a.from = a.to;
                    a.to = a.to + (a.fast ? 0.12f : 0.24f);
                    if (!stopSnd) snd(audio::SND_REELSTOP);   // la oprirea rapida, un singur "tac"
                    if (a.fast) stopSnd = true;
                    for (int r = 0; r < game->rows; ++r)
                        if (isScatterKind(game->symbols[(size_t)result.raw[c][r]].kind) || game->symbols[(size_t)result.raw[c][r]].kind == KIND_COLLECTOR) {
                            snd(audio::SND_SCATTER);
                            break;
                        }
                }
                break;
            }
            case 4: {
                a.t += dt;
                float u = std::min(1.0f, a.t / (a.fast ? 0.08f : 0.16f));
                a.pos = a.from + (a.to - a.from) * easeInOut(u);
                a.speed = 0;
                if (u >= 1.0f) {
                    a.pos = (float)posMod(a.target, L);
                    a.phase = 0;
                    a.t = 0;
                }
                break;
            }
            default:
                a.t += dt;
                break;
        }
        if (a.phase != 0) all = false;
    }
    // sunetul de asteptare cand o rola mai are de adus al treilea scatter
    bool waiting = false;
    for (int c = 0; c < game->reels; ++c) {
        if (!reels[c].anticipation || reels[c].phase == 0 || reels[c].phase == 4) continue;
        bool prevStopped = true;
        for (int q = 0; q < c; ++q)
            if (reels[q].phase != 0 && reels[q].phase != 4) prevStopped = false;
        if (prevStopped) waiting = true;
        break;
    }
    (void)waiting;   // fara sunet de asteptare in fundal
    if (!waiting && anticVoice) {
        audio::stop(anticVoice);
        anticVoice = 0;
    }
    if (all) {
        audio::stop(spinVoice);
        audio::stop(anticVoice);
        spinVoice = anticVoice = 0;
        spinning = false;
        onReelsStopped();
    }
}

void SlotMachineUI::Impl::onReelsStopped() {
    idleT = 0;
    if (fsActive && game->fsType == FS_COLLECT) {
        const WinLine* cw = nullptr;
        for (size_t i = 0; i < result.wins.size(); ++i)
            if (result.wins[i].line == -3) cw = &result.wins[i];
        if (cw) {
            // pescarul cu cosul: pestii se lumineaza, apoi zboara pe rand in cos
            collectN = 0;
            for (int c = 0; c < game->reels; ++c)
                for (int r = 0; r < game->rows; ++r)
                    if (result.money[c][r] > 0 && collectN < MAX_REELS * MAX_ROWS) {
                        collectCells[collectN][0] = c;
                        collectCells[collectN][1] = r;
                        ++collectN;
                    }
            collectMult = std::max(1, cw->count);
            collectT = 0;
            snd(audio::SND_SCATTER);
            return;
        }
    }
    if (fsActive && game->fsType == FS_EXPANDING) {
        int n = 0;
        for (int c = 0; c < game->reels; ++c)
            if (result.specialExpanded[c]) ++n;
        if (n > 0) {
            expandT = 0;
            expandReels = n;
            return;
        }
    }
    beginPresentation();
}

void SlotMachineUI::Impl::beginPresentation() {
    presenting = !result.wins.empty();
    presentT = 0;
    idleT = 0;
    double tot = result.total;
    float dur = 0.6f + (float)std::min(2.0, tot / std::max(0.01, bet()) * 0.04);
    if (speedNow() == 1) dur *= 0.6f;
    else if (speedNow() == 2) dur *= 0.35f;
    if (fsActive) {
        double old = fsWon;
        fsWon += tot;
        if (tot > 0) setWinCounter(old, fsWon, dur);
    } else {
        pendingWin = tot;
        if (tot > 0) setWinCounter(0, tot, dur);
        else {
            winShown = winTarget = 0;
            message.clear();
        }
    }
    if (result.fsAwarded > 0) {
        if (fsActive) {
            fsLeft += result.fsAwarded;
            fsTotal += result.fsAwarded;
            fsRetriggerT = 0;
            snd(audio::SND_FSINTRO, 0.7f);
        } else {
            fsPendingAward = result.fsAwarded;
        }
    }
    if (!fsActive && tot >= bet() * 15.0) {
        bigWinT = 0;
        bigWinAmount = tot;
        spawnCoins(30, 640, 420, 500);
        snd(audio::SND_BIGWIN);
        snd(audio::SND_COINS);
    } else if (tot > 0) {
        snd(tot >= bet() * 5.0 ? audio::SND_WIN2 : audio::SND_WIN1);
    }
    buying = false;
}

float SlotMachineUI::Impl::presentationMinTime() const {
    static const float WIN_T[3] = {1.9f, 0.9f, 0.6f};
    int sp = speedNow();
    if (!haveResult || result.wins.empty()) return fsActive ? (sp ? 0.35f : 0.6f) : 0.3f;
    return fsActive ? (sp ? WIN_T[sp] : 1.5f) : WIN_T[sp];
}

void SlotMachineUI::Impl::startFreeSpins() {
    if (overlay != OV_FS_INTRO) return;
    overlay = OV_NONE;
    fsActive = true;
    fsLeft = fsTotal = fsPendingAward;
    fsPendingAward = 0;
    fsPlayed = 0;
    fsWon = 0;
    presenting = false;
    winShown = winTarget = 0;
    winCountT = 1;
    idleT = 0;
}

void SlotMachineUI::Impl::endFreeSpins() {
    if (overlay != OV_FS_OUTRO) return;
    overlay = OV_NONE;
    fsActive = false;
    double before = pendingWin;
    pendingWin += fsWon;
    setWinCounter(before, pendingWin, 0.8f);
    fsWon = 0;
    special = -1;
    presenting = false;
    message = SLOT_U8("SPECIALA S-A ÎNCHEIAT");
    idleT = 0;
}

void SlotMachineUI::Impl::startGamble() {
    if (!game || overlay != OV_NONE || spinning || fsActive || pendingWin <= 0 || bigWinT >= 0 || expandT >= 0 || collectT >= 0 ||
        fsPendingAward > 0 || jpTierPending >= 0)
        return;
    autoPlay = false;
    overlay = OV_GAMBLE;
    overlayT = 0;
    gambleAmount = pendingWin;
    pendingWin = 0;
    gambleSteps = 0;
    gambleCard = -1;
    gambleFlipT = -1;
    gambleResult = 0;
    gambleResultT = 0;
    presenting = false;
}

void SlotMachineUI::Impl::gamble(bool red) {
    if (overlay != OV_GAMBLE || gambleFlipT >= 0 || gambleAmount <= 0) return;
    if (gambleSteps >= GAMBLE_MAX_STEPS || gambleAmount * 2 > GAMBLE_LIMIT) return;
    int suit = engine.randInt(4);
    gambleCard = suit;
    gambleRank = engine.randInt(13);
    gambleFlipT = 0;
    gambleResultT = 0;
    snd(audio::SND_CARD);
    bool win = (suit < 2) == red;
    gambleResult = win ? 1 : 2;
    for (int i = 5; i > 0; --i) gambleHistory[i] = gambleHistory[i - 1];
    gambleHistory[0] = suit;
    gambleHistCount = std::min(6, gambleHistCount + 1);
    if (win) {
        gambleAmount *= 2;
        ++gambleSteps;
    } else {
        gambleAmount = 0;
    }
}

void SlotMachineUI::Impl::finishGamble() {
    if (overlay != OV_GAMBLE) return;
    if (gambleFlipT >= 0 && gambleFlipT < 0.45f) return;
    wallet->balance += gambleAmount;
    if (gambleAmount > 0) snd(audio::SND_COINS);
    winShown = winTarget = gambleAmount;
    winCountT = 1;
    message = gambleAmount > 0 ? SLOT_U8("CÂȘTIG COLECTAT: ") + lei(gambleAmount) : SLOT_U8("AI PIERDUT LA DUBLAJ");
    gambleAmount = 0;
    pendingWin = 0;
    overlay = OV_NONE;
    idleT = 0;
}

void SlotMachineUI::Impl::openJackpot(int tier) {
    snd(audio::SND_JACKPOT);
    overlay = OV_JACKPOT;
    overlayT = 0;
    jpTier = tier;
    jpAmount = jackpots().take(tier);
    jpPaid = false;
    jpPicks = 0;
    jpDoneT = -1;
    jpSel = 0;
    for (int i = 0; i < 12; ++i) jpCards[i] = -1;
    // secventa predeterminata: nivelul castigat apare de 3 ori, celelalte de cel mult 2 ori
    std::vector<int> seq;
    for (int t = 0; t < 4; ++t) {
        if (t == tier) continue;
        int n = engine.randInt(3);
        for (int k = 0; k < n; ++k) seq.push_back(t);
    }
    seq.push_back(tier);
    seq.push_back(tier);
    for (int i = (int)seq.size() - 1; i > 0; --i) std::swap(seq[(size_t)i], seq[(size_t)engine.randInt(i + 1)]);
    seq.push_back(tier);
    jpSeqLen = (int)seq.size();
    for (int i = 0; i < jpSeqLen && i < 12; ++i) jpSeq[i] = seq[(size_t)i];
}

void SlotMachineUI::Impl::pickJackpot(int card) {
    if (overlay != OV_JACKPOT || jpDoneT >= 0 || card < 0 || card >= 12 || jpCards[card] >= 0) return;
    jpCards[card] = jpSeq[jpPicks++];
    snd(audio::SND_CARD);
    if (jpPicks >= jpSeqLen) {
        snd(audio::SND_BIGWIN);
        snd(audio::SND_COINS);
        jpDoneT = 0;
        wallet->balance += jpAmount;
        jpPaid = true;
        spawnCoins(70, 640, 380, 700);
        int counts[4] = {0, 0, 0, 0};
        for (int i = 0; i < 12; ++i)
            if (jpCards[i] >= 0) counts[jpCards[i]]++;
        for (int i = 0; i < 12; ++i) {
            if (jpCards[i] >= 0) continue;
            int t = engine.randInt(4), guard = 0;
            while ((t == jpTier || counts[t] >= 2) && guard++ < 16) t = engine.randInt(4);
            counts[t]++;
            jpCards[i] = t + 4;   // +4 = carte dezvaluita la final (afisata estompat)
        }
    }
}

void SlotMachineUI::Impl::updateGame(float dt) {
    const float prevExpand = expandT, prevOverlayT = overlayT, prevFlip = gambleFlipT;
    if (spinning) updateReels(dt);
    else
        for (int c = 0; c < game->reels; ++c) reels[c].t += dt;
    if (expandT >= 0) {
        expandT += dt;
        for (int k = 0; k < expandReels; ++k) {
            float st = k * 0.5f;
            if (prevExpand <= st && expandT > st) snd(audio::SND_EXPAND);
        }
        if (expandT >= 0.5f * expandReels + 0.4f) {
            expandT = -1;
            beginPresentation();
        }
    }
    if (collectT >= 0) {
        float prev = collectT;
        collectT += dt;
        if (prev < 0.45f && collectT >= 0.45f) snd(audio::SND_EXPAND, 0.6f);
        for (int k = 0; k < collectN; ++k) {
            float arrive = collectFlyStart(k) + 0.55f;
            if (prev < arrive && collectT >= arrive) snd(audio::SND_COINS, 0.45f);
        }
        float mt = collectFlyStart(collectN > 0 ? collectN - 1 : 0) + 0.85f;
        if (collectMult > 1 && prev < mt && collectT >= mt) snd(audio::SND_WIN2, 0.7f);
        if (collectT >= collectEnd()) {
            collectT = -1;
            fishCollected = true;
            beginPresentation();
        }
    }
    if (presenting) presentT += dt;
    if (winCountT < 1.0f) {
        winCountT = std::min(1.0f, winCountT + dt / winCountDur);
        winShown = winFrom + (winTarget - winFrom) * easeOutCubic(winCountT);
        if (winCountT >= 1.0f && countVoice) {
            audio::stop(countVoice);
            countVoice = 0;
        }
    }
    overlayT += dt;
    if (overlay == OV_FS_INTRO && game->fsType == FS_EXPANDING && special >= 0 && prevOverlayT < 2.4f && overlayT >= 2.4f)
        snd(audio::SND_SCATTER);
    if (bigWinT >= 0) {
        bigWinT += dt;
        if (bigWinT < 3.0f && std::rand() % 5 == 0) spawnCoins(1, 640, 700, 1200);
        static const float BIGWIN_T[3] = {6.0f, 3.5f, 2.2f};
        if (bigWinT > BIGWIN_T[speedNow()]) bigWinT = -1;
    }
    if (fsRetriggerT >= 0) {
        fsRetriggerT += dt;
        if (fsRetriggerT > 2.4f) fsRetriggerT = -1;
    }
    if (overlay == OV_GAMBLE && gambleFlipT >= 0) {
        gambleFlipT += dt;
        if (gambleFlipT >= 0.45f) {
            gambleResultT += dt;
            if (gambleResult == 2 && gambleResultT > 1.5f) finishGamble();
            else if (gambleResult == 1 && gambleResultT > 1.0f) {
                if (gambleSteps >= GAMBLE_MAX_STEPS || gambleAmount * 2 > GAMBLE_LIMIT) finishGamble();
                else {
                    gambleFlipT = -1;
                    gambleCard = -1;
                    gambleResult = 0;
                }
            }
        }
    }
    if (overlay == OV_GAMBLE && prevFlip >= 0 && prevFlip < 0.45f && gambleFlipT >= 0.45f)
        snd(gambleResult == 1 ? audio::SND_GWIN : audio::SND_GLOSE);
    if (overlay == OV_FS_INTRO && overlayT > 6.0f) startFreeSpins();
    if (overlay == OV_FS_OUTRO && overlayT > 5.5f) endFreeSpins();
    if (overlay == OV_JACKPOT && jpDoneT >= 0) {
        jpDoneT += dt;
        if (jpDoneT > 7.0f) overlay = OV_NONE;
    }

    // progresia automata (dupa ce s-a vazut castigul)
    bool idle = !spinning && expandT < 0 && collectT < 0 && bigWinT < 0 && overlay == OV_NONE;
    if (!idle) {
        idleT = 0;
        return;
    }
    idleT += dt;
    float minT = presenting ? presentationMinTime() : (fsActive ? 0.5f : 0.3f);
    if (idleT < minT) return;
    if (jpTierPending >= 0) {
        int t = jpTierPending;
        jpTierPending = -1;
        openJackpot(t);
    } else if (fsPendingAward > 0) {
        overlay = OV_FS_INTRO;
        overlayT = 0;
        snd(audio::SND_FSINTRO);
        special = game->fsType == FS_EXPANDING ? engine.pickSpecialSymbol() : -1;
        spawnCoins(20, 640, 600, 800);
    } else if (fsActive) {
        if (fsLeft > 0) startSpin();
        else {
            overlay = OV_FS_OUTRO;
            overlayT = 0;
            spawnCoins(40, 640, 600, 900);
            snd(audio::SND_FSEND);
            snd(audio::SND_COINS);
        }
    } else if (autoPlay && idleT >= minT + (autoSpeed == 0 ? 0.15f : 0.02f)) {
        if (autoLeft == 0) {
            autoPlay = false;
        } else {
            if (autoLeft > 0) --autoLeft;
            startSpin();
            if (!spinning) autoPlay = false;    // credit insuficient etc.
        }
    }
}

void SlotMachineUI::Impl::gameKey(int key) {
    switch (overlay) {
        case OV_INFO:
            if (key == KEY_LEFT) doAction(ACT_INFO_PREV, 0);
            else if (key == KEY_RIGHT) doAction(ACT_INFO_NEXT, 0);
            else if (key == KEY_ESCAPE || key == KEY_BACKSPACE || key == KEY_I || key == KEY_ENTER || key == KEY_SPACE)
                overlay = OV_NONE;
            return;
        case OV_AUTO:
            if (key == KEY_LEFT) autoSel = (autoSel + AUTO_OPTION_COUNT - 1) % AUTO_OPTION_COUNT;
            else if (key == KEY_RIGHT || key == KEY_TAB) autoSel = (autoSel + 1) % AUTO_OPTION_COUNT;
            else if (key == KEY_UP) autoSel = std::max(0, autoSel - 4);
            else if (key == KEY_DOWN) autoSel = std::min(AUTO_OPTION_COUNT - 1, autoSel + 4);
            else if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE) startAuto(autoSel);
            else if (key == KEY_S) autoSpeed = (autoSpeed + 1) % 3;
            else if (key == KEY_ESCAPE || key == KEY_BACKSPACE || key == KEY_A) overlay = OV_NONE;
            return;
        case OV_BUY:
            if (key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_SPACE || key == KEY_B) startBuy();
            else if (key == KEY_ESCAPE || key == KEY_BACKSPACE || key == KEY_N) overlay = OV_NONE;
            return;
        case OV_GAMBLE:
            if (key == KEY_LEFT || key == KEY_R) gamble(true);
            else if (key == KEY_RIGHT || key == KEY_N) gamble(false);
            else if (key == KEY_SPACE || key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_C || key == KEY_ESCAPE || key == KEY_G)
                finishGamble();
            return;
        case OV_FS_INTRO:
        case OV_FS_OUTRO:
            if (key == KEY_SPACE || key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_ESCAPE) doAction(ACT_CONTINUE, 0);
            return;
        case OV_JACKPOT:
            if (jpDoneT >= 0) {
                if (key == KEY_SPACE || key == KEY_ENTER || key == KEY_KP_ENTER || key == KEY_ESCAPE) doAction(ACT_CONTINUE, 0);
                return;
            }
            if (key == KEY_LEFT) jpSel = (jpSel + 11) % 12;
            else if (key == KEY_RIGHT) jpSel = (jpSel + 1) % 12;
            else if (key == KEY_UP) jpSel = (jpSel + 8) % 12;
            else if (key == KEY_DOWN) jpSel = (jpSel + 4) % 12;
            else if (key == KEY_ENTER || key == KEY_KP_ENTER) pickJackpot(jpSel);
            else if (key == KEY_SPACE) {
                std::vector<int> free;
                for (int i = 0; i < 12; ++i)
                    if (jpCards[i] < 0) free.push_back(i);
                if (!free.empty()) pickJackpot(free[(size_t)engine.randInt((int)free.size())]);
            }
            return;
        default: break;
    }
    switch (key) {
        case KEY_SPACE:
        case KEY_ENTER:
        case KEY_KP_ENTER:
        case KEY_S:
            if (bigWinT >= 0) doAction(ACT_CONTINUE, 0);
            else if (spinning) quickStop();
            else startSpin();
            break;
        case KEY_UP:
        case KEY_EQUAL:
        case KEY_KP_ADD: changeBet(1); break;
        case KEY_DOWN:
        case KEY_MINUS:
        case KEY_KP_SUBTRACT: changeBet(-1); break;
        case KEY_A: autoButton(); break;
        case KEY_G:
        case KEY_D: startGamble(); break;
        case KEY_I: doAction(ACT_INFO, 0); break;
        case KEY_B: doAction(ACT_BUY, 0); break;
        case KEY_ESCAPE:
        case KEY_BACKSPACE:
        case KEY_M:
            if (bigWinT >= 0) doAction(ACT_CONTINUE, 0);
            else if (!busy()) leaveGame();
            break;
        default: break;
    }
}

// ================================================================== desen
void SlotMachineUI::Impl::computeHighlights() {
    for (int c = 0; c < MAX_REELS; ++c)
        for (int r = 0; r < MAX_ROWS; ++r) hl[c][r] = false;
    hlOn = presenting && !spinning && expandT < 0 && collectT < 0 && haveResult && !result.wins.empty();
    activeWin = -1;
    if (!hlOn) return;
    int n = (int)result.wins.size();
    if (presentT >= 1.3f && n > 1) activeWin = (int)((presentT - 1.3f) / 1.3f) % n;
    for (int i = 0; i < n; ++i) {
        if (activeWin >= 0 && i != activeWin) continue;
        const WinLine& w = result.wins[(size_t)i];
        for (size_t k = 0; k < w.cells.size(); ++k) hl[w.cells[k].reel][w.cells[k].row] = true;
    }
    if (fishCollected)   // pestii din cos nu mai sunt pe role
        for (int q = 0; q < collectN; ++q) hl[collectCells[q][0]][collectCells[q][1]] = false;
}

void SlotMachineUI::Impl::drawBackground(const Theme& th, Rect area, float strength) {
    Color accent = hexc(th.accent);
    G.rectGrad(area, hexc(th.bgTop), hexc(th.bgBottom));
    G.rays(centered(640, 300, 1700, 1700), withA(accent, 0.07f * strength), withA(accent, 0.0f), 16, time * 0.05f);
    switch (th.style) {
        case 1:   // jar si flacari
            for (int i = 0; i < 26; ++i) {
                float t = std::fmod(time * (0.12f + 0.08f * hash01(i)) + hash01(i + 50), 1.0f);
                float x = hash01(i + 100) * 1280.0f + std::sin(time * 1.5f + i) * 18.0f;
                float y = 560.0f - t * 560.0f;
                G.circle(x, y, 2.0f + 2.5f * hash01(i + 7), withA(hexc(0xffa030), (1.0f - t) * 0.7f * strength));
            }
            for (int i = 0; i < 14; ++i) {
                float fh = 90.0f + 30.0f * std::sin(time * 5.0f + i * 1.7f);
                G.pushAlpha(0.45f * strength);
                G.sprite(SPR_FLAME, R(i * 96.0f - 20.0f, 548.0f - fh, 110.0f, fh + 20.0f));
                G.popAlpha();
            }
            break;
        case 2:   // stele
        case 4:   // gheata / ceata
            for (int i = 0; i < 34; ++i) {
                float tw = 0.5f + 0.5f * std::sin(time * (1.5f + hash01(i) * 2.0f) + i * 3.1f);
                float x = hash01(i + 200) * 1280.0f, y = hash01(i + 300) * 540.0f;
                float s = 10.0f + 18.0f * hash01(i + 400);
                G.pushAlpha(tw * 0.75f * strength);
                G.sprite(SPR_SPARKLE, centered(x, y, s, s));
                G.popAlpha();
            }
            if (th.style == 4) {
                for (int i = 0; i < 5; ++i) {
                    float x = std::fmod(hash01(i + 9) * 1280.0f + time * (12.0f + i * 5.0f), 1600.0f) - 160.0f;
                    G.radial(centered(x, 120.0f + i * 90.0f, 520, 160), withA(hexc(0xffffff), 0.05f * strength), hexA(0xffffff, 0.0f));
                }
            }
            break;
        case 3:   // bule
            for (int i = 0; i < 24; ++i) {
                float t = std::fmod(time * (0.06f + 0.05f * hash01(i)) + hash01(i + 70), 1.0f);
                float x = hash01(i + 170) * 1280.0f + std::sin(time + i) * 12.0f;
                float y = 560.0f - t * 600.0f;
                float r = 4.0f + 9.0f * hash01(i + 270);
                G.ring(x, y, r, 1.8f, withA(hexc(0xffffff), 0.35f * (1.0f - t) * strength));
                G.circle(x - r * 0.35f, y - r * 0.35f, r * 0.22f, withA(hexc(0xffffff), 0.4f * (1.0f - t) * strength));
            }
            break;
        default: break;
    }
    G.radial(centered(640, 330, 1700, 1100), hexA(0x000000, 0.0f), hexA(0x000000, 0.55f));
}

void SlotMachineUI::Impl::drawSymbol(int sym, Rect r, float alpha, float blur, float scale) {
    const SymbolDef& sd = game->symbols[(size_t)sym];
    float size = std::min(r.w, r.h) - 6.0f;
    Rect sr = centered(r.cx(), r.cy(), size * scale, size * scale);
    Color mul = rgba(1, 1, 1, alpha);
    if (sd.tint) {
        Color t = hexc(sd.tint);
        G.sprite(sd.sprite, sr, mul, blur, &t);
    } else {
        G.sprite(sd.sprite, sr, mul, blur);
    }
}

void SlotMachineUI::Impl::drawReels() {
    const Theme& th = game->theme;
    Rect win = reelWindow();
    Color frame = hexc(th.frame), accent = hexc(th.accent);
    const float ch = win.h / game->rows;
    // rama
    G.softRect(R(win.x - 16, win.y - 10, win.w + 32, win.h + 34), 24, 22, hexA(0x000000, 0.75f));
    G.rectGrad(inset(win, -15), lerpc(frame, hexc(0xffffff), 0.35f), lerpc(frame, hexc(0x000000), 0.5f), 18);
    G.rectGrad(inset(win, -11), lerpc(frame, hexc(0x000000), 0.15f), lerpc(frame, hexc(0x000000), 0.6f), 15);
    G.rect(inset(win, -4), hexA(0x000000, 0.92f), 9);
    G.rectGrad(win, hexc(th.reelTop), hexc(th.reelBottom), 6);
    bool lightReels = ((th.reelTop >> 16) & 255) > 160;
    G.pushClip(win);
    // separatoare intre role
    for (int c = 1; c < game->reels; ++c) {
        float x = win.x + c * win.w / game->reels;
        G.rectGrad(R(x - 2, win.y, 4, win.h), hexA(0x000000, lightReels ? 0.18f : 0.45f), hexA(0x000000, lightReels ? 0.18f : 0.45f));
    }
    float pulse = 1.0f + 0.07f * std::sin(time * 9.0f);
    for (int c = 0; c < game->reels; ++c) {
        const ReelAnim& a = reels[c];
        const std::vector<int>& strip = engine.stripFor(c, reelFs);
        const int L = (int)strip.size();
        float base = std::floor(a.pos);
        float frac = a.pos - base;
        int b = (int)base;
        float blur = (a.phase >= 1 && a.phase <= 3) ? clampf(a.speed / SPIN_SPEED, 0, 1) * 0.32f : 0.0f;
        bool settled = a.phase == 0;
        for (int k = -1; k <= game->rows; ++k) {
            int sym = strip[(size_t)posMod(b + k, L)];
            if (settled && k >= 0 && k < game->rows && game->symbols[(size_t)sym].kind == KIND_MONEY && (collectT >= 0 || fishCollected)) {
                bool hideFish = false;   // pestele a zburat in cosul pescarului
                for (int q = 0; q < collectN; ++q)
                    if (collectCells[q][0] == c && collectCells[q][1] == k && (fishCollected || collectT >= collectFlyStart(q))) hideFish = true;
                if (hideFish) continue;
            }
            Rect cr = cellRect(c, 0);
            cr.y = win.y + (k - frac) * ch;
            float alpha = 1.0f, sc = 1.0f;
            if (settled && hlOn && k >= 0 && k < game->rows) {
                if (hl[c][k]) {
                    sc = pulse;
                    int wi = activeWin;
                    Color gc = wi >= 0 && result.wins[(size_t)wi].line >= 0 ? lineColor(result.wins[(size_t)wi].line) : accent;
                    G.softRect(inset(cr, 10), 16, 18, withA(gc, 0.55f));
                } else {
                    alpha = 0.42f;
                }
            }
            drawSymbol(sym, cr, alpha, blur, sc);
            if (game->symbols[(size_t)sym].kind == KIND_MONEY) {
                float mv = engine.moneyAt(c, b + k, reelFs);
                if (mv > 0 && blur < 0.2f) drawMoneyTag(cr, mv * bet(), alpha * (1.0f - blur * 4.0f), sc);
            }
        }
        // wild extins pe toata rola
        if (haveResult && settled && result.wildExpanded[c]) {
            float p = easeOutCubic(clampf(a.t / 0.35f, 0, 1));
            Rect col = R(win.x + c * win.w / game->reels, win.y, win.w / game->reels, win.h);
            Rect grow = centered(col.cx(), col.cy(), col.w, col.h * p);
            G.rectGrad(inset(grow, 3), withA(accent, 0.55f), withA(lerpc(accent, hexc(0x000000), 0.6f), 0.75f), 10);
            G.pushClip(grow);
            for (int r = 0; r < game->rows; ++r) {
                int wsym = result.grid[c][r];
                float a2 = (hlOn && !hl[c][r]) ? 0.5f : 1.0f;
                drawSymbol(wsym, cellRect(c, r), a2, 0, hlOn && hl[c][r] ? pulse : 1.0f);
            }
            G.popClip();
            G.frame(inset(grow, 3), 10, 3, hexc(th.frame));
        }
        // simbolul special care se extinde (rotiri speciale)
        if (haveResult && settled && fsActive && special >= 0 && result.specialExpanded[c] && (expandT >= 0 || presenting)) {
            int order = 0;
            for (int q = 0; q < c; ++q)
                if (result.specialExpanded[q]) ++order;
            float p = expandT >= 0 ? clampf((expandT - order * 0.5f) / 0.45f, 0, 1) : 1.0f;
            if (p > 0) {
                Rect col = R(win.x + c * win.w / game->reels, win.y, win.w / game->reels, win.h);
                float e = easeOutBack(p);
                Rect grow = centered(col.cx(), col.cy(), col.w, std::min(col.h, col.h * e));
                G.softRect(inset(grow, 2), 12, 16, withA(hexc(0xffd040), 0.6f * p));
                G.rectGrad(inset(grow, 3), hexc(0xfff0b0), hexc(0xc08010), 10);
                G.rays(inset(grow, 3), hexA(0xffffff, 0.35f), hexA(0xffffff, 0.0f), 10, time * 0.8f, 10);
                G.pushClip(grow);
                for (int r = 0; r < game->rows; ++r) {
                    float s2 = (presenting && hlOn && hl[c][r]) ? pulse : 1.0f;
                    drawSymbol(special, cellRect(c, r), 1.0f, 0, s2);
                }
                G.popClip();
                G.frame(inset(grow, 3), 10, 3.5f, hexc(0xfff2b0));
                if (p < 1.0f) G.sprite(SPR_SPARKLE, centered(col.cx(), col.cy(), 160 * p, 160 * p));
            }
        }
    }
    // umbrire cilindrica sus / jos
    G.rectGrad(R(win.x, win.y, win.w, 34), hexA(0x000000, lightReels ? 0.28f : 0.5f), hexA(0x000000, 0.0f));
    G.rectGrad(R(win.x, win.bottom() - 34, win.w, 34), hexA(0x000000, 0.0f), hexA(0x000000, lightReels ? 0.28f : 0.5f));
    G.popClip();
    // asteptare dupa al treilea scatter
    for (int c = 0; c < game->reels; ++c) {
        const ReelAnim& a = reels[c];
        if (!a.anticipation || a.phase == 0 || !spinning) continue;
        Rect col = R(win.x + c * win.w / game->reels, win.y, win.w / game->reels, win.h);
        float pl = 0.6f + 0.4f * std::sin(time * 14.0f);
        G.softRect(inset(col, -4), 10, 16, withA(hexc(0xffd040), 0.55f * pl));
        G.frame(inset(col, 2), 8, 4, withA(hexc(0xfff2a0), pl));
    }
}

void SlotMachineUI::Impl::drawWinPresentation() {
    if (!hlOn) return;
    Rect win = reelWindow();
    const std::vector<std::vector<int> >& lines = paylines(game->reels, game->rows);
    int n = (int)result.wins.size();
    for (int i = 0; i < n; ++i) {
        if (activeWin >= 0 && i != activeWin) continue;
        const WinLine& w = result.wins[(size_t)i];
        Color col = w.line >= 0 ? lineColor(w.line) : hexc(0xffd040);
        bool single = activeWin >= 0;
        if (w.line >= 0) {
            const std::vector<int>& p = lines[(size_t)w.line];
            float px[7], py[7];
            int np = 0;
            Rect c0 = cellRect(0, p[0]);
            px[np] = win.x - 10;
            py[np++] = c0.cy();
            for (int c = 0; c < game->reels; ++c) {
                Rect cr = cellRect(c, p[(size_t)c]);
                px[np] = cr.cx();
                py[np++] = cr.cy();
            }
            Rect cl = cellRect(game->reels - 1, p[(size_t)(game->reels - 1)]);
            px[np] = win.right() + 10;
            py[np++] = cl.cy();
            float th = single ? 6.0f : 4.0f;
            for (int k = 0; k + 1 < np; ++k) G.line(px[k], py[k], px[k + 1], py[k + 1], th + 10, withA(col, 0.22f));
            for (int k = 0; k + 1 < np; ++k) G.line(px[k], py[k], px[k + 1], py[k + 1], th + 3.5f, hexA(0x000000, 0.65f));
            for (int k = 0; k + 1 < np; ++k) G.line(px[k], py[k], px[k + 1], py[k + 1], th, col);
        }
        for (size_t k = 0; k < w.cells.size(); ++k) {
            if (w.line == -3 && !hl[w.cells[k].reel][w.cells[k].row]) continue;   // peste deja in cos
            Rect cr = inset(cellRect(w.cells[k].reel, w.cells[k].row), 5);
            G.frame(cr, 12, single ? 4.5f : 3.0f, col);
        }
        if (single && !w.cells.empty()) {
            const Cell& last = w.cells.back();
            Rect cr = cellRect(last.reel, last.row);
            std::string s = fmtMoney(w.amount);
            float tw = G.textWidth(s.c_str(), FONT_DISPLAY, 24) + 26;
            Rect pill = centered(cr.cx(), cr.bottom() - 18, tw, 34);
            G.softRect(pill, 17, 8, hexA(0x000000, 0.6f));
            G.rectBorder(pill, hexc(0x2a1a00), hexc(0x000000), 17, 2.0f, col);
            TextStyle ts(FONT_DISPLAY, 24, hexc(0xffffff));
            ts.grad(hexc(0xffffff), hexc(0xffe080)).al(ALIGN_CENTER);
            G.textBox(pill, s.c_str(), ts);
        }
    }
}

void SlotMachineUI::Impl::drawLineIndicators() {
    if (game->rows != 3 || game->lines > 20) return;
    Rect win = reelWindow();
    const std::vector<std::vector<int> >& lines = paylines(game->reels, game->rows);
    for (int side = 0; side < 2; ++side) {
        int reel = side == 0 ? 0 : game->reels - 1;
        float x = side == 0 ? win.x - 36 : win.right() + 36;
        int count[3] = {0, 0, 0}, idx[20];
        for (int l = 0; l < game->lines; ++l) {
            int r = lines[(size_t)l][(size_t)reel];
            idx[l] = count[r]++;
        }
        for (int l = 0; l < game->lines; ++l) {
            int r = lines[(size_t)l][(size_t)reel];
            Rect cr = cellRect(reel, r);
            float y = cr.cy() + (idx[l] - (count[r] - 1) * 0.5f) * 21.0f;
            bool on = false;
            if (hlOn) {
                for (size_t i = 0; i < result.wins.size(); ++i)
                    if (result.wins[i].line == l && (activeWin < 0 || activeWin == (int)i)) on = true;
            }
            Color c = lineColor(l);
            Rect b = centered(x, y, on ? 36.0f : 30.0f, on ? 20.0f : 18.0f);
            if (on) G.softRect(b, 10, 8, withA(c, 0.8f));
            G.rectBorder(b, brighten(c, 1.1f), lerpc(c, hexc(0x000000), 0.45f), 9, 1.2f, hexA(0x000000, 0.7f));
            char num[16];
            std::snprintf(num, sizeof(num), "%d", l + 1);
            TextStyle ts(FONT_UI, on ? 13.0f : 12.0f, hexc(0xffffff));
            ts.al(ALIGN_CENTER).stroke(1.4f, hexA(0x000000, 0.8f));
            G.textBox(b, num, ts);
        }
    }
}

void SlotMachineUI::Impl::drawPanel() {
    const Theme& th = game->theme;
    Rect pr = R(0, 548, CANVAS_W, 172);
    G.softRect(R(0, 538, CANVAS_W, 20), 0, 10, hexA(0x000000, 0.6f));
    G.rectGrad(pr, hexc(0x241c32), hexc(0x07050b));
    G.pattern(pr, hexA(0x000000, 0.0f), hexA(0xffffff, 0.03f), 28);
    G.rectGrad(R(0, 548, CANVAS_W, 3), hexc(0xffe07a), hexc(0x8a5a10));

    // banda de mesaje
    Rect ms = R(24, 558, 1076, 32);
    G.rect(ms, hexA(0x000000, 0.5f), 16);
    std::string msg = message, right;
    if (fsActive) {
        char b[96];
        std::snprintf(b, sizeof(b), SLOT_U8("ROTIRI SPECIALE: %d / %d"), std::min(fsPlayed, fsTotal), fsTotal);
        msg = b;
        if (special >= 0) right = "SIMBOL SPECIAL";
        else if (game->fsMultiplier > 1) {
            std::snprintf(b, sizeof(b), SLOT_U8("CÂȘTIGURI x%d"), game->fsMultiplier);
            right = b;
        }
    } else if (hlOn) {
        if (activeWin >= 0) {
            const WinLine& w = result.wins[(size_t)activeWin];
            char b[96];
            if (w.line >= 0) std::snprintf(b, sizeof(b), "LINIA %d:  %s", w.line + 1, lei(w.amount).c_str());
            else if (w.line == -1) std::snprintf(b, sizeof(b), "SCATTER x%d:  %s", w.count, lei(w.amount).c_str());
            else if (w.line == -3) std::snprintf(b, sizeof(b), SLOT_U8("PESCARUL STRÂNGE x%d:  %s"), w.count, lei(w.amount).c_str());
            else std::snprintf(b, sizeof(b), "SPECIALA x%d:  %s", w.count, lei(w.amount).c_str());
            msg = b;
        } else {
            msg = SLOT_U8("CÂȘTIG: ") + lei(result.total);
        }
    } else if (!spinning && pendingWin > 0 && overlay == OV_NONE) {
        msg = SLOT_U8("START = COLECTEAZĂ   •   DUBLAJ = DUBLEAZĂ CÂȘTIGUL");
    } else if (!spinning && msg.empty()) {
        msg = std::fmod(time, 1.6f) < 1.1f ? SLOT_U8("APASĂ START PENTRU A JUCA") : "";
    }
    if (autoPlay && !fsActive) {
        char ab[64];
        static const char* SP[3] = {"", SLOT_U8("  •  RAPID"), SLOT_U8("  •  TURBO")};
        if (autoLeft >= 0) std::snprintf(ab, sizeof(ab), SLOT_U8("AUTO: MAI SUNT %d ROTIRI%s"), autoLeft, SP[autoSpeed]);
        else std::snprintf(ab, sizeof(ab), SLOT_U8("AUTO: PÂNĂ LA OPRIRE%s"), SP[autoSpeed]);
        right = ab;
    }
    TextStyle mt(FONT_UI, 18, hexc(0xfff2c0));
    mt.al(ALIGN_CENTER).drop(1.5f).sp(0.6f);
    G.textBox(R(ms.x + 20, ms.y, ms.w - 40, ms.h), msg.c_str(), mt, 10);
    if (!right.empty()) {
        TextStyle rt(FONT_UI, 14, hexc(0xffd060));
        rt.al(ALIGN_RIGHT);
        G.textBox(R(ms.right() - 260, ms.y, 200, ms.h), right.c_str(), rt);
        if (special >= 0) drawSymbol(special, centered(ms.right() - 34, ms.cy(), 44, 44), 1, 0, 1);
    }

    // contoare
    drawCreditBox(R(24, 598, 270, 62), "CREDIT", creditShown, false);
    Rect bb = R(306, 598, 318, 62);
    G.softRect(R(bb.x, bb.y + 3, bb.w, bb.h), 10, 6, hexA(0x000000, 0.5f));
    G.rectBorder(bb, hexc(0x110d1a), hexc(0x050409), 10, 1.6f, hexA(0xd0a848, 0.85f));
    bool betOk = !spinning && !fsActive && overlay == OV_NONE && bigWinT < 0 && expandT < 0 && collectT < 0 && fsPendingAward == 0;
    button(R(312, 604, 54, 50), "-", ACT_BET_MINUS, 0, BTN_DARK, betOk && betStep > minBetStep(*game), false, 30);
    button(R(564, 604, 54, 50), "+", ACT_BET_PLUS, 0, BTN_DARK, betOk && betStep < BET_STEP_COUNT - 1, false, 28);
    char lb[64];
    std::snprintf(lb, sizeof(lb), "PARIU  •  %d LINII", game->lines);
    TextStyle bl(FONT_UI, 12, hexc(0xffd060));
    bl.al(ALIGN_CENTER).sp(1.0f);
    G.textBox(R(372, 604, 186, 16), lb, bl);
    TextStyle bv(FONT_DISPLAY, 26, hexc(0xffffff));
    bv.al(ALIGN_CENTER).drop(1.5f);
    G.textBox(R(372, 622, 186, 32), lei(bet()).c_str(), bv, 12);
    drawCreditBox(R(636, 598, 270, 62), fsActive ? SLOT_U8("CÂȘTIG SPECIALE") : SLOT_U8("CÂȘTIG"), winShown,
                  (hlOn || pendingWin > 0) && winShown > 0);
    Rect lbx = R(918, 598, 170, 62);
    G.rectBorder(lbx, hexc(0x110d1a), hexc(0x050409), 10, 1.6f, hexA(0xd0a848, 0.6f));
    TextStyle l1(FONT_UI, 12, hexc(0xffd060));
    l1.al(ALIGN_CENTER).sp(1.0f);
    G.textBox(R(lbx.x, lbx.y + 6, lbx.w, 16), "PE LINIE", l1);
    TextStyle l2(FONT_DISPLAY, 21, hexc(0xffffff));
    l2.al(ALIGN_CENTER);
    char lineB[32];
    double lbv = lineBet();
    if (lbv < 0.1) std::snprintf(lineB, sizeof(lineB), "%.3f", lbv);
    else std::snprintf(lineB, sizeof(lineB), "%.2f", lbv);
    for (char* q = lineB; *q; ++q)
        if (*q == '.') *q = ',';
    G.textBox(R(lbx.x + 8, lbx.y + 24, lbx.w - 16, 32), lineB, l2);

    // butoane mici
    bool idle = !busy();
    button(R(24, 670, 112, 40), "MENIU", ACT_MENU, 0, BTN_DARK, idle);
    button(R(146, 670, 112, 40), "INFO", ACT_INFO, 0, BTN_DARK, !spinning && overlay == OV_NONE);
    {
        char al[32];
        if (autoPlay && autoLeft >= 0) std::snprintf(al, sizeof(al), "AUTO %d", autoLeft);
        else if (autoPlay) std::snprintf(al, sizeof(al), SLOT_U8("AUTO ∞"));
        else std::snprintf(al, sizeof(al), "AUTO");
        button(R(268, 670, 112, 40), al, ACT_AUTO, 0, autoPlay ? BTN_GOLD : BTN_DARK, (!fsActive && overlay == OV_NONE) || autoPlay, autoPlay, autoPlay ? 15 : 0);
    }
    bool gambleOk = pendingWin > 0 && !spinning && !fsActive && overlay == OV_NONE && bigWinT < 0 && expandT < 0 && collectT < 0 &&
                    fsPendingAward == 0 && jpTierPending < 0 && pendingWin * 2 <= GAMBLE_LIMIT;
    bool blink = std::fmod(time, 0.8f) < 0.5f;
    button(R(390, 670, 150, 40), "DUBLAJ", ACT_GAMBLE, 0, gambleOk ? BTN_RED : BTN_DARK, gambleOk, gambleOk && blink);
    if (game->buyCost > 0) {
        std::string lbl = SLOT_U8("CUMPĂRĂ SPECIALA  ") + lei(bet() * game->buyCost);
        button(R(550, 670, 356, 40), lbl.c_str(), ACT_BUY, 0, BTN_GOLD, idle && pendingWin <= 0 && overlay == OV_NONE, false, 15);
    }

    // START
    float cx = 1180, cy = 634, rad = 62;
    bool canPress = overlay == OV_NONE && (spinning || bigWinT >= 0 || !fsActive);
    Rect sr = centered(cx, cy, rad * 2, rad * 2);
    bool hov = canPress && hovered(sr);
    if (canPress) addRegion(sr, ACT_START, 0);
    Color c1, c2;
    const char* lbl = "START";
    if (spinning) {
        c1 = hexc(0xff8a3a);
        c2 = hexc(0xa02000);
        lbl = "STOP";
    } else if (pendingWin > 0) {
        c1 = hexc(0xffe68a);
        c2 = hexc(0xc07a0a);
    } else {
        c1 = hexc(0x6af08a);
        c2 = hexc(0x0a7a2a);
    }
    if (hov) {
        c1 = brighten(c1, 1.15f);
        c2 = brighten(c2, 1.15f);
    }
    float gp = (!spinning && canPress) ? 0.5f + 0.5f * std::sin(time * 4.0f) : 0.3f;
    G.softRect(sr, rad, 22, withA(c1, 0.35f + 0.3f * gp));
    G.circleGrad(cx, cy + 4, rad, hexA(0x000000, 0.6f), hexA(0x000000, 0.6f));
    G.circleGrad(cx, cy, rad, hexc(0xfff2c0), hexc(0x8a5a10));
    G.circleGrad(cx, cy, rad - 6, c1, c2);
    G.rectGrad(centered(cx, cy - rad * 0.42f, rad * 1.3f, rad * 0.75f), hexA(0xffffff, 0.45f), hexA(0xffffff, 0.02f), rad * 0.37f);
    TextStyle st(FONT_DISPLAY, 28, hexc(0xffffff));
    st.al(ALIGN_CENTER).stroke(2.5f, hexA(0x000000, 0.55f)).drop(2.0f);
    G.pushAlpha(canPress ? 1.0f : 0.5f);
    G.text(lbl, cx, cy, st);
    G.popAlpha();
    (void)th;
}

void SlotMachineUI::Impl::drawGame() {
    const Theme& th = game->theme;
    const Rect full = R(0, 0, CANVAS_W, CANVAS_H);
    computeHighlights();
    drawBackground(th, full, 1.0f);
    if (fsActive) G.rect(full, withA(hexc(th.accent), 0.10f + 0.04f * std::sin(time * 2.0f)));

    // titlu si jackpot
    TextStyle tt(th.titleFont, th.titleFont == FONT_SERIF ? 42.0f : 40.0f, hexc(th.titleTop));
    tt.grad(hexc(th.titleTop), hexc(th.titleBottom)).stroke(3.0f, hexc(th.titleOutline)).drop(3.0f).halo(8, withA(hexc(th.accent), 0.5f)).al(ALIGN_CENTER);
    G.textBox(R(336, 10, 608, 60), game->name.c_str(), tt, 20);
    drawJackpotBadge(R(14, 12, 150, 52), 0, 0);
    drawJackpotBadge(R(170, 12, 150, 52), 1, 0);
    drawJackpotBadge(R(960, 12, 150, 52), 2, 0);
    drawJackpotBadge(R(1116, 12, 150, 52), 3, 0);

    drawReels();
    drawWinPresentation();
    drawLineIndicators();
    if (fsActive && game->fsType == FS_COLLECT && (collectT >= 0 || (fishCollected && presenting))) drawCollect();
    if (fsRetriggerT >= 0) {
        float a = clampf(fsRetriggerT * 4.0f, 0, 1) * clampf((2.4f - fsRetriggerT) * 3.0f, 0, 1);
        G.pushAlpha(a);
        TextStyle rt(FONT_DISPLAY, 54, hexc(0xfff6c0));
        char b[64];
        std::snprintf(b, sizeof(b), "+%d ROTIRI SPECIALE", game->fsCount);
        rt.grad(hexc(0xfff6c0), hexc(0xffa020)).stroke(4, hexc(0x3a1400)).halo(10, hexA(0xffb020, 0.7f)).al(ALIGN_CENTER);
        G.text(b, 640, 300, rt);
        G.popAlpha();
    }
    drawPanel();

    drawingOverlay = true;
    if (bigWinT >= 0) drawBigWin();
    switch (overlay) {
        case OV_FS_INTRO: drawFsIntro(); break;
        case OV_FS_OUTRO: drawFsOutro(); break;
        case OV_GAMBLE: drawGamble(); break;
        case OV_INFO: drawInfo(); break;
        case OV_BUY: drawBuy(); break;
        case OV_AUTO: drawAutoPanel(); break;
        case OV_JACKPOT: drawJackpot(); break;
        default: break;
    }
    drawingOverlay = false;
}

// ================================================================== ferestre
static void panelCard(Renderer2D& G, Rect r, const Theme& th, float radius) {
    G.softRect(R(r.x, r.y + 10, r.w, r.h), radius, 26, hexA(0x000000, 0.75f));
    G.rectGrad(inset(r, -6), lerpc(hexc(th.frame), hexc(0xffffff), 0.3f), lerpc(hexc(th.frame), hexc(0x000000), 0.5f), radius + 6);
    G.rectGrad(r, lerpc(hexc(th.bgTop), hexc(0xffffff), 0.08f), hexc(th.bgBottom), radius);
    G.pattern(r, hexA(0x000000, 0.0f), hexA(0xffffff, 0.035f), 26, radius);
}

void SlotMachineUI::Impl::drawFsIntro() {
    const Theme& th = game->theme;
    float t = overlayT;
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.7f * clampf(t * 3, 0, 1)));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_CONTINUE, 0);
    float s = easeOutBack(clampf(t * 2.2f, 0, 1));
    G.pushTransformAround(640, 330, s);
    G.rays(centered(640, 330, 1200, 1200), withA(hexc(0xffd040), 0.22f), hexA(0xffd040, 0.0f), 16, time * 0.3f);
    Rect card = centered(640, 330, 720, 440);
    panelCard(G, card, th, 26);
    TextStyle a(FONT_DISPLAY, 70, hexc(0xfff6c0));
    a.grad(hexc(0xfff6c0), hexc(0xffa020)).stroke(4, hexc(0x3a1400)).drop(4).halo(12, hexA(0xffb020, 0.7f)).al(ALIGN_CENTER);
    G.text("SPECIALA!", 640, 168, a);
    char b[96];
    std::snprintf(b, sizeof(b), SLOT_U8("AI CÂȘTIGAT %d ROTIRI SPECIALE"), fsPendingAward);
    TextStyle sb(FONT_UI, 26, hexc(0xffffff));
    sb.al(ALIGN_CENTER).drop(2);
    G.text(b, 640, 228, sb);
    if (game->fsType == FS_EXPANDING && special >= 0) {
        TextStyle lt(FONT_UI, 16, hexc(0xffd060));
        lt.al(ALIGN_CENTER).sp(2.0f);
        G.text("SIMBOLUL SPECIAL ESTE", 640, 272, lt);
        Rect box = centered(640, 380, 190, 170);
        G.softRect(box, 18, 16, hexA(0x000000, 0.6f));
        G.rectGrad(box, hexc(th.reelTop), hexc(th.reelBottom), 18);
        G.frame(box, 18, 4, hexc(0xffd040));
        const float landAt = 2.4f;
        int shown = special;
        if (t < landAt) {
            std::vector<int> normals;
            for (size_t i = 0; i < game->symbols.size(); ++i)
                if (game->symbols[i].kind == KIND_NORMAL) normals.push_back((int)i);
            float ph = 16.0f * t - 3.2f * t * t;
            shown = normals[(size_t)((int)ph % (int)normals.size())];
            G.pushClip(box);
            drawSymbol(shown, inset(box, 8), 1.0f, 0.12f, 1.0f);
            G.popClip();
        } else {
            float k = clampf((t - landAt) * 3.0f, 0, 1);
            G.softRect(box, 18, 30, hexA(0xffd040, 0.7f * (1.0f - k) + 0.25f));
            drawSymbol(special, inset(box, 4), 1.0f, 0, 1.0f + 0.25f * (1.0f - k));
            G.sprite(SPR_SPARKLE, centered(box.x + 30, box.y + 30, 60, 60), rgba(1, 1, 1, 0.5f + 0.5f * std::sin(time * 6)));
            TextStyle et(FONT_UI, 19, hexc(0xffffff));
            et.al(ALIGN_CENTER).drop(1.5f);
            G.text(SLOT_U8("se extinde pe toată rola și plătește pe toate liniile"), 640, 492, et);
        }
    } else if (game->fsType == FS_COLLECT) {
        Rect box = centered(640, 375, 190, 170);
        G.softRect(box, 18, 30, hexA(0x40d0ff, 0.45f + 0.15f * std::sin(time * 4)));
        G.rectGrad(box, hexc(th.reelTop), hexc(th.reelBottom), 18);
        G.frame(box, 18, 4, hexc(0xffd040));
        int fisher = -1;
        for (size_t i = 0; i < game->symbols.size(); ++i)
            if (game->symbols[i].kind == KIND_COLLECTOR) fisher = (int)i;
        if (fisher >= 0) drawSymbol(fisher, inset(box, 4), 1.0f, 0, 1.0f + 0.04f * std::sin(time * 5));
        TextStyle et(FONT_UI, 20, hexc(0xffffff));
        et.al(ALIGN_CENTER).drop(1.5f);
        G.text(SLOT_U8("pescarul apare pe role și strânge valorile tuturor peștilor"), 640, 492, et);
    } else {
        char m[32];
        std::snprintf(m, sizeof(m), "x%d", game->fsMultiplier);
        TextStyle mt(FONT_DISPLAY, 120, hexc(0xffffff));
        mt.grad(hexc(0xffffff), hexc(0xffc040)).stroke(5, hexc(0x3a1400)).halo(14, hexA(0xffb020, 0.7f)).al(ALIGN_CENTER);
        G.text(m, 640, 375, mt);
        TextStyle et(FONT_UI, 22, hexc(0xffffff));
        et.al(ALIGN_CENTER).drop(1.5f);
        G.text(SLOT_U8("toate câștigurile din rotirile speciale sunt multiplicate"), 640, 482, et);
    }
    if (t > 2.6f && std::fmod(time, 1.0f) < 0.65f) {
        TextStyle pt(FONT_UI, 18, hexc(0xffe080));
        pt.al(ALIGN_CENTER).sp(2);
        G.text(SLOT_U8("APASĂ START"), 640, 528, pt);
    }
    G.popTransform();
}

void SlotMachineUI::Impl::drawFsOutro() {
    const Theme& th = game->theme;
    float t = overlayT;
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.7f * clampf(t * 3, 0, 1)));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_CONTINUE, 0);
    float s = easeOutBack(clampf(t * 2.2f, 0, 1));
    G.pushTransformAround(640, 330, s);
    G.rays(centered(640, 330, 1200, 1200), withA(hexc(0xffd040), 0.25f), hexA(0xffd040, 0.0f), 16, -time * 0.3f);
    Rect card = centered(640, 330, 700, 380);
    panelCard(G, card, th, 26);
    TextStyle a(FONT_UI, 26, hexc(0xffffff));
    a.al(ALIGN_CENTER).drop(2).sp(1.5f);
    G.text(SLOT_U8("SPECIALA S-A ÎNCHEIAT"), 640, 200, a);
    TextStyle b(FONT_UI, 18, hexc(0xffd060));
    b.al(ALIGN_CENTER).sp(2);
    G.text(SLOT_U8("CÂȘTIG TOTAL"), 640, 262, b);
    double shown = fsWon * easeOutCubic(clampf(t / 2.0f, 0, 1));
    TextStyle c(FONT_DISPLAY, 84, hexc(0xfff6c0));
    c.grad(hexc(0xfff6c0), hexc(0xffa020)).stroke(4, hexc(0x3a1400)).drop(4).halo(12, hexA(0xffb020, 0.7f)).al(ALIGN_CENTER);
    G.textBox(R(300, 290, 680, 110), lei(shown).c_str(), c, 30);
    char d[80];
    std::snprintf(d, sizeof(d), "rotiri jucate: %d", fsPlayed);
    TextStyle e(FONT_UI, 18, hexA(0xffffff, 0.8f));
    e.al(ALIGN_CENTER);
    G.text(d, 640, 432, e);
    if (t > 1.5f && std::fmod(time, 1.0f) < 0.65f) {
        TextStyle pt(FONT_UI, 18, hexc(0xffe080));
        pt.al(ALIGN_CENTER).sp(2);
        G.text(SLOT_U8("APASĂ START"), 640, 482, pt);
    }
    G.popTransform();
}

void SlotMachineUI::Impl::drawBigWin() {
    float t = bigWinT;
    float a = clampf(t * 4.0f, 0, 1) * clampf((6.0f - t) * 2.0f, 0, 1);
    G.pushAlpha(a);
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.55f));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_CONTINUE, 0);
    G.rays(centered(640, 300, 1500, 1500), hexA(0xffd040, 0.28f), hexA(0xffd040, 0.0f), 18, time * 0.5f);
    G.radial(centered(640, 300, 900, 600), hexA(0xffe080, 0.35f), hexA(0xffe080, 0.0f));
    double x = bigWinAmount / std::max(0.01, bet());
    const char* title = x >= 100 ? SLOT_U8("CÂȘTIG FANTASTIC!") : (x >= 50 ? SLOT_U8("SUPER CÂȘTIG!") : SLOT_U8("CÂȘTIG MARE!"));
    float s = easeOutBack(clampf(t * 1.8f, 0, 1)) * (1.0f + 0.03f * std::sin(time * 6.0f));
    G.pushTransformAround(640, 300, s);
    TextStyle ts(FONT_DISPLAY, 92, hexc(0xfff6c0));
    ts.grad(hexc(0xfffbe0), hexc(0xffa020)).stroke(5, hexc(0x3a1000)).drop(5).halo(14, hexA(0xff9020, 0.8f)).al(ALIGN_CENTER);
    G.textBox(R(140, 180, 1000, 120), title, ts, 40);
    double shown = bigWinAmount * easeOutCubic(clampf(t / 3.0f, 0, 1));
    TextStyle vs(FONT_DISPLAY, 76, hexc(0xffffff));
    vs.grad(hexc(0xffffff), hexc(0xffe080)).stroke(4, hexc(0x3a1000)).drop(4).al(ALIGN_CENTER);
    G.textBox(R(240, 300, 800, 100), lei(shown).c_str(), vs, 30);
    G.popTransform();
    if (t > 3.2f && std::fmod(time, 1.0f) < 0.65f) {
        TextStyle pt(FONT_UI, 18, hexc(0xffe080));
        pt.al(ALIGN_CENTER).sp(2);
        G.text(SLOT_U8("APASĂ START"), 640, 450, pt);
    }
    G.popAlpha();
}

void SlotMachineUI::Impl::drawCard(Rect r, int suit, int rank, float flip) {
    float w = r.w * std::max(0.0f, flip);
    if (w < 1.0f) return;
    Rect cr = R(r.cx() - w * 0.5f, r.y, w, r.h);
    G.softRect(R(cr.x, cr.y + 7, cr.w, cr.h), 14, 10, hexA(0x000000, 0.6f));
    if (suit < 0) {
        G.rectBorder(cr, hexc(0xc0142e), hexc(0x5a0010), 12, 4, hexc(0xffd060));
        G.pattern(inset(cr, 10), hexA(0x000000, 0.0f), hexA(0xffd060, 0.35f), 16, 8);
        G.frame(inset(cr, 10), 8, 1.5f, hexA(0xffd060, 0.7f));
        float s = std::min(cr.w * 0.55f, 80.0f);
        G.sprite(SPR_SUNDISK, centered(cr.cx(), cr.cy(), s, s));
        return;
    }
    G.rectBorder(cr, hexc(0xffffff), hexc(0xe6e6ee), 12, 2, hexc(0xb8b8c4));
    int spr = SPR_SUIT_HEART + suit;
    float s = std::min(cr.w * 0.62f, r.w * 0.62f);
    G.sprite(spr, centered(cr.cx(), cr.cy() + 6, s, s));
    if (flip > 0.6f) {
        Color rc = suit < 2 ? hexc(0xd0102a) : hexc(0x101018);
        TextStyle ts(FONT_SERIF, 30, rc);
        G.text(RANKS[rank % 13], cr.x + 12, cr.y + 26, ts);
        ts.al(ALIGN_RIGHT);
        G.text(RANKS[rank % 13], cr.right() - 12, cr.bottom() - 24, ts);
        G.sprite(spr, R(cr.x + 10, cr.y + 44, 26, 26));
    }
}

void SlotMachineUI::Impl::drawGamble() {
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.6f));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_BLOCK, 0);
    Rect area = R(140, 70, 1000, 470);
    G.softRect(R(area.x, area.y + 10, area.w, area.h), 26, 26, hexA(0x000000, 0.75f));
    G.rectGrad(inset(area, -6), hexc(0xffe07a), hexc(0x7a4a08), 30);
    G.rectGrad(area, hexc(0x14804a), hexc(0x064022), 26);
    G.pattern(area, hexA(0x000000, 0.0f), hexA(0xffffff, 0.035f), 30, 26);
    G.radial(centered(640, 300, 900, 520), hexA(0xffffff, 0.12f), hexA(0xffffff, 0.0f));
    TextStyle tt(FONT_DISPLAY, 46, hexc(0xfff6c0));
    tt.grad(hexc(0xfff6c0), hexc(0xffb020)).stroke(3.5f, hexc(0x2a1000)).drop(3).al(ALIGN_CENTER);
    G.text("DUBLAJ", 640, 112, tt);
    TextStyle sub(FONT_UI, 16, hexA(0xffffff, 0.85f));
    sub.al(ALIGN_CENTER);
    bool showResult = gambleResult != 0 && gambleFlipT >= 0.45f;
    if (!showResult) G.text(SLOT_U8("Ghicește culoarea cărții: ROȘU sau NEGRU"), 640, 152, sub);
    // istoric
    TextStyle ht(FONT_UI, 12, hexc(0xffe080));
    ht.al(ALIGN_RIGHT).sp(1);
    G.text(SLOT_U8("ULTIMELE CĂRȚI"), 1110, 100, ht);
    for (int i = 0; i < 6; ++i) {
        Rect mc = R(1110 - 6 * 38 + i * 38, 112, 32, 42);
        G.rectBorder(mc, hexA(0xffffff, i < gambleHistCount ? 1.0f : 0.15f), hexA(0xe0e0e8, i < gambleHistCount ? 1.0f : 0.15f), 6, 1, hexA(0x808090, 0.8f));
        if (i < gambleHistCount) G.sprite(SPR_SUIT_HEART + gambleHistory[i], inset(mc, 5));
    }
    // cartea
    Rect cr = centered(640, 312, 156, 216);
    float flip = 1.0f;
    int suit = gambleCard;
    if (gambleFlipT >= 0 && gambleFlipT < 0.45f) {
        float u = gambleFlipT / 0.45f;
        flip = std::fabs(std::cos(u * PI_F));
        if (u < 0.5f) suit = -1;
    }
    drawCard(cr, suit, gambleRank, flip);
    if (showResult) {
        bool won = gambleResult == 1;
        TextStyle rt(FONT_DISPLAY, 38, won ? hexc(0x8affa0) : hexc(0xff7070));
        rt.stroke(3.5f, hexA(0x000000, 0.8f)).drop(3).al(ALIGN_CENTER);
        float s = easeOutBack(clampf(gambleResultT * 3.0f, 0, 1));
        G.pushTransformAround(640, 160, s);
        G.text(won ? SLOT_U8("CÂȘTIGAT!") : "PIERDUT", 640, 160, rt);
        G.popTransform();
    }
    bool canChoose = gambleFlipT < 0 && gambleAmount > 0 && gambleSteps < GAMBLE_MAX_STEPS && gambleAmount * 2 <= GAMBLE_LIMIT;
    button(R(232, 262, 230, 100), SLOT_U8("ROȘU"), ACT_GAMBLE_RED, 0, BTN_RED, canChoose, false, 34);
    button(R(818, 262, 230, 100), "NEGRU", ACT_GAMBLE_BLACK, 0, BTN_BLACK, canChoose, false, 34);
    TextStyle k(FONT_UI, 13, hexA(0xffffff, 0.7f));
    k.al(ALIGN_CENTER);
    G.text(SLOT_U8("tasta R  /  săgeată stânga"), 347, 380, k);
    G.text(SLOT_U8("tasta N  /  săgeată dreapta"), 933, 380, k);
    // sume
    TextStyle lab(FONT_UI, 14, hexc(0xffe080));
    lab.al(ALIGN_CENTER).sp(1.5f);
    TextStyle val(FONT_DISPLAY, 30, hexc(0xffffff));
    val.al(ALIGN_CENTER).drop(2);
    G.text(SLOT_U8("MIZĂ DUBLAJ"), 347, 432, lab);
    G.text(lei(gambleAmount).c_str(), 347, 466, val);
    G.text(SLOT_U8("CÂȘTIG POSIBIL"), 933, 432, lab);
    G.text(lei(gambleAmount * 2).c_str(), 933, 466, val);
    char st[64];
    std::snprintf(st, sizeof(st), SLOT_U8("dublări rămase: %d"), std::max(0, GAMBLE_MAX_STEPS - gambleSteps));
    G.text(st, 640, 448, k);
    button(R(520, 474, 240, 52), SLOT_U8("COLECTEAZĂ"), ACT_GAMBLE_COLLECT, 0, BTN_GOLD,
           gambleFlipT < 0 || gambleFlipT >= 0.45f, false, 20);
}

void SlotMachineUI::Impl::drawInfo() {
    const Theme& th = game->theme;
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.8f));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_BLOCK, 0);
    Rect panel = R(50, 36, 1180, 650);
    panelCard(G, panel, th, 22);
    TextStyle tt(th.titleFont, 34, hexc(th.titleTop));
    tt.grad(hexc(th.titleTop), hexc(th.titleBottom)).stroke(2.5f, hexc(th.titleOutline)).drop(2).al(ALIGN_CENTER);
    G.textBox(R(200, 50, 880, 50), game->name.c_str(), tt, 16);
    button(R(1164, 50, 50, 44), SLOT_U8("×"), ACT_INFO_CLOSE, 0, BTN_DARK, true, false, 30);
    if (infoPage == 0) {
        TextStyle h(FONT_UI, 15, hexc(0xffd060));
        h.al(ALIGN_CENTER).sp(2);
        char hb[96];
        std::snprintf(hb, sizeof(hb), SLOT_U8("TABEL DE PLĂȚI  •  PARIU %s"), lei(bet()).c_str());
        G.text(hb, 640, 112, h);
        int n = (int)game->symbols.size();
        int cols = 4;
        for (int i = 0; i < n; ++i) {
            int idx = n - 1 - i;   // simbolurile speciale si cele mari primele
            const SymbolDef& sd = game->symbols[(size_t)idx];
            int col = i % cols, row = i / cols;
            Rect cell = R(76.0f + col * 284.0f, 132.0f + row * 160.0f, 270, 148);
            G.rect(cell, hexA(0x000000, 0.35f), 14);
            G.frame(cell, 14, 1.2f, withA(hexc(th.frame), 0.5f));
            drawSymbol(idx, R(cell.x + 4, cell.y + 14, 120, 120), 1, 0, 1);
            float ty = cell.y + 30;
            TextStyle pt(FONT_UI, 18, hexc(0xffffff));
            TextStyle pn(FONT_DISPLAY, 18, hexc(0xffd060));
            bool any = false;
            if (sd.kind == KIND_WILD || sd.kind == KIND_BOOK) {
                TextStyle wt(FONT_UI, 13, hexc(0x9af0ff));
                G.text(sd.kind == KIND_BOOK ? "WILD + SCATTER" : (game->expandingWild ? "WILD EXTINS" : "WILD"), cell.x + 128, cell.y + 22, wt);
                ty += 12;
            } else if (sd.kind == KIND_SCATTER) {
                TextStyle wt(FONT_UI, 13, hexc(0x9af0ff));
                G.text("SCATTER", cell.x + 128, cell.y + 22, wt);
                ty += 12;
            } else if (sd.kind == KIND_MONEY || sd.kind == KIND_COLLECTOR) {
                TextStyle wt(FONT_UI, 13, hexc(0x9af0ff));
                TextStyle ws(FONT_UI, 13, hexA(0xffffff, 0.85f));
                bool mny = sd.kind == KIND_MONEY;
                G.text(mny ? SLOT_U8("PEȘTE CU BANI") : "PESCAR", cell.x + 128, cell.y + 22, wt);
                G.text(mny ? SLOT_U8("valori de 2× până la") : SLOT_U8("doar în rotirile"), cell.x + 130, cell.y + 60, ws);
                G.text(mny ? SLOT_U8("50× pariul total") : SLOT_U8("speciale; strânge"), cell.x + 130, cell.y + 80, ws);
                G.text(mny ? SLOT_U8("(îi strânge pescarul)") : SLOT_U8("toți peștii"), cell.x + 130, cell.y + 100, ws);
                continue;
            }
            for (int k = 5; k >= 2; --k) {
                if (k > game->reels) continue;
                if (sd.pay[k] <= 0) continue;
                double v = sd.pay[k] * game->payScale * ((sd.kind == KIND_SCATTER || sd.kind == KIND_BOOK) ? bet() : lineBet());
                char a[16];
                std::snprintf(a, sizeof(a), "%d×", k);
                G.text(a, cell.x + 130, ty, pn);
                G.text(fmtMoney(v).c_str(), cell.x + 170, ty, pt);
                ty += 26;
                any = true;
            }
            if (!any) {
                TextStyle wt(FONT_UI, 13, hexA(0xffffff, 0.8f));
                G.text(SLOT_U8("înlocuiește"), cell.x + 130, cell.y + 70, wt);
                G.text("simbolurile", cell.x + 130, cell.y + 90, wt);
            }
        }
    } else {
        std::vector<std::string> L;
        L.push_back(SLOT_U8("• Câștigurile se plătesc de la stânga la dreapta, pe liniile active."));
        L.push_back(SLOT_U8("• Pe o linie se plătește doar cel mai mare câștig."));
        bool hasWild = false, hasScatter = false;
        for (size_t i = 0; i < game->symbols.size(); ++i) {
            if (game->symbols[i].kind == KIND_WILD) hasWild = true;
            if (game->symbols[i].kind == KIND_SCATTER) hasScatter = true;
        }
        if (hasWild) L.push_back(game->fsType == FS_COLLECT ? SLOT_U8("• Peștele WILD înlocuiește simbolurile, mai puțin colacul și peștii cu bani.")
                                                              : SLOT_U8("• WILD înlocuiește toate simbolurile, mai puțin scatter-ul."));
        if (game->expandingWild) L.push_back(SLOT_U8("• WILD apare pe rolele 2, 3, 4 și se extinde pe toată rola."));
        if (hasScatter) L.push_back(SLOT_U8("• Scatter-ul plătește oriunde pe ecran, înmulțit cu pariul total."));
        char b[160];
        if (game->fsType == FS_EXPANDING) {
            L.push_back(SLOT_U8("• Simbolul special e WILD și SCATTER; 3 sau mai multe aduc speciala."));
            std::snprintf(b, sizeof(b), SLOT_U8("• Speciala: %d rotiri gratuite cu un simbol ales care se extinde"), game->fsCount);
            L.push_back(b);
            L.push_back(SLOT_U8("   pe toată rola și plătește pe toate liniile. Se poate reactiva."));
        } else if (game->fsType == FS_MULTIPLIER) {
            std::snprintf(b, sizeof(b), SLOT_U8("• 3 sau mai multe scatter-e aduc %d rotiri cu câștigurile x%d."), game->fsCount, game->fsMultiplier);
            L.push_back(b);
        } else if (game->fsType == FS_COLLECT) {
            std::snprintf(b, sizeof(b), SLOT_U8("• 3 sau mai multe colace BONUS aduc %d rotiri speciale."), game->fsCount);
            L.push_back(b);
            L.push_back(SLOT_U8("• Peștii au valori în bani. În speciale apare PESCARUL,"));
            L.push_back(SLOT_U8("   care strânge valoarea tuturor peștilor de pe ecran."));
        }
        if (game->buyCost > 0) {
            std::snprintf(b, sizeof(b), SLOT_U8("• Speciala se poate cumpăra cu %d × pariul total."), game->buyCost);
            L.push_back(b);
        }
        L.push_back(SLOT_U8("• DUBLAJ: ghicește culoarea cărții ca să-ți dublezi câștigul."));
        L.push_back(SLOT_U8("• Jackpot-ul are 4 niveluri, comune tuturor aparatelor."));
        L.push_back(SLOT_U8("• Procent teoretic de returnare: aproximativ 96%."));
        TextStyle rt(FONT_UI, 16, hexc(0xffffff));
        for (size_t i = 0; i < L.size(); ++i) G.textBox(R(90, 128.0f + i * 34.0f, 560, 30), L[i].c_str(), rt, 11);
        // diagrama liniilor
        const std::vector<std::vector<int> >& lines = paylines(game->reels, game->rows);
        int nl = std::min(game->lines, 20);
        TextStyle lh(FONT_UI, 15, hexc(0xffd060));
        lh.al(ALIGN_CENTER).sp(2);
        char lb[64];
        std::snprintf(lb, sizeof(lb), "LINII DE PLATĂ (%d)", game->lines);
        G.text(lb, 935, 118, lh);
        for (int l = 0; l < nl; ++l) {
            int col = l % 4, row = l / 4;
            Rect g = R(690.0f + col * 124.0f, 136.0f + row * 100.0f, 112, 88);
            G.rect(g, hexA(0x000000, 0.4f), 8);
            float cw = 100.0f / game->reels, chh = 64.0f / game->rows;
            for (int c = 0; c < game->reels; ++c)
                for (int r = 0; r < game->rows; ++r) {
                    bool on = lines[(size_t)l][(size_t)c] == r;
                    G.rect(R(g.x + 6 + c * cw + 1, g.y + 18 + r * chh + 1, cw - 2, chh - 2), on ? lineColor(l) : hexA(0xffffff, 0.12f), 2);
                }
            char nb[16];
            std::snprintf(nb, sizeof(nb), "%d", l + 1);
            TextStyle nt(FONT_UI, 12, hexc(0xffffff));
            G.text(nb, g.x + 7, g.y + 9, nt);
        }
        if (game->lines > nl) {
            std::snprintf(lb, sizeof(lb), SLOT_U8("+ încă %d linii"), game->lines - nl);
            TextStyle mt(FONT_UI, 14, hexA(0xffffff, 0.8f));
            mt.al(ALIGN_CENTER);
            G.text(lb, 935, 650, mt);
        }
    }
    button(R(470, 628, 64, 44), "", ACT_INFO_PREV, 0, BTN_GLASS, infoPage > 0);
    G.triangle(502, 650, 18, 0, infoPage > 0 ? hexc(0xffffff) : hexA(0xffffff, 0.3f));
    button(R(746, 628, 64, 44), "", ACT_INFO_NEXT, 0, BTN_GLASS, infoPage < 1);
    G.triangle(778, 650, 18, 1, infoPage < 1 ? hexc(0xffffff) : hexA(0xffffff, 0.3f));
    char pg[32];
    std::snprintf(pg, sizeof(pg), "PAGINA %d / 2", infoPage + 1);
    TextStyle pt(FONT_UI, 15, hexc(0xffffff));
    pt.al(ALIGN_CENTER);
    G.text(pg, 640, 650, pt);
}

void SlotMachineUI::Impl::drawBuy() {
    const Theme& th = game->theme;
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.65f));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_BLOCK, 0);
    float s = easeOutBack(clampf(overlayT * 3.0f, 0, 1));
    G.pushTransformAround(640, 330, s);
    Rect d = centered(640, 330, 640, 330);
    panelCard(G, d, th, 24);
    TextStyle tt(FONT_DISPLAY, 40, hexc(0xfff6c0));
    tt.grad(hexc(0xfff6c0), hexc(0xffb020)).stroke(3, hexc(0x2a1000)).drop(3).al(ALIGN_CENTER);
    G.text(SLOT_U8("CUMPĂRĂ SPECIALA"), 640, 214, tt);
    double price = bet() * game->buyCost;
    char a[128];
    std::snprintf(a, sizeof(a), SLOT_U8("Primești direct %d rotiri speciale."), game->fsCount);
    TextStyle bt(FONT_UI, 20, hexc(0xffffff));
    bt.al(ALIGN_CENTER).drop(1.5f);
    G.text(a, 640, 268, bt);
    std::snprintf(a, sizeof(a), SLOT_U8("Preț: %d × pariul (%s)"), game->buyCost, lei(bet()).c_str());
    G.text(a, 640, 302, bt);
    TextStyle pv(FONT_DISPLAY, 44, hexc(0xffffff));
    pv.grad(hexc(0xffffff), hexc(0xffe080)).stroke(3, hexc(0x2a1000)).al(ALIGN_CENTER);
    G.text(lei(price).c_str(), 640, 352, pv);
    bool ok = wallet->balance + 1e-9 >= price;
    if (!ok) {
        TextStyle wt(FONT_UI, 16, hexc(0xff8080));
        wt.al(ALIGN_CENTER);
        G.text("CREDIT INSUFICIENT", 640, 392, wt);
    }
    button(R(370, 418, 250, 60), SLOT_U8("DA, CUMPĂR"), ACT_BUY_YES, 0, BTN_GREEN, ok, false, 22);
    button(R(660, 418, 250, 60), SLOT_U8("RENUNȚ"), ACT_BUY_NO, 0, BTN_RED, true, false, 22);
    G.popTransform();
}

void SlotMachineUI::Impl::drawJackpot() {
    static const uint32_t TIER_COL[4] = {0xf0a060, 0xdfe8f8, 0xffd040, 0x8af0ff};
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x05020c, 0.88f));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), jpDoneT > 0.8f ? ACT_CONTINUE : ACT_BLOCK, 0);
    G.rays(centered(640, 320, 1500, 1500), hexA(0xffd040, 0.18f), hexA(0xffd040, 0.0f), 20, time * 0.25f);
    TextStyle tt(FONT_DISPLAY, 58, hexc(0xfff6c0));
    tt.grad(hexc(0xfffbe0), hexc(0xffa020)).stroke(4, hexc(0x2a1000)).drop(4).halo(12, hexA(0xff9020, 0.7f)).al(ALIGN_CENTER);
    G.text("JACKPOT MISTERIOS!", 640, 58, tt);
    TextStyle st(FONT_UI, 18, hexc(0xffffff));
    st.al(ALIGN_CENTER).drop(1.5f);
    if (jpDoneT < 0) G.text(SLOT_U8("Întoarce cărțile până găsești 3 la fel"), 640, 104, st);
    for (int i = 0; i < 12; ++i) {
        int col = i % 4, row = i / 4;
        Rect cr = R(310.0f + col * 170.0f, 128.0f + row * 146.0f, 150, 130);
        int v = jpCards[i];
        bool sel = keyboardFocus && i == jpSel && jpDoneT < 0;
        if (v < 0) {
            bool hov = jpDoneT < 0 && hovered(cr);
            if (jpDoneT < 0) addRegion(cr, ACT_JP_PICK, i);
            if (hov || sel) G.softRect(inset(cr, -4), 16, 14, hexA(0xb080ff, 0.7f));
            G.softRect(R(cr.x, cr.y + 6, cr.w, cr.h), 14, 10, hexA(0x000000, 0.6f));
            G.rectBorder(cr, hexc(0x6a2ad0), hexc(0x2a0a5a), 14, 3, hexc(0xffd060));
            G.pattern(inset(cr, 8), hexA(0x000000, 0.0f), hexA(0xffd060, 0.25f), 14, 8);
            TextStyle q(FONT_DISPLAY, 54, hexc(0xffe080));
            q.stroke(3, hexc(0x2a1000)).al(ALIGN_CENTER);
            G.text("?", cr.cx(), cr.cy(), q);
        } else {
            int tier = v % 4;
            bool late = v >= 4;
            bool win = jpDoneT >= 0 && tier == jpTier && !late;
            G.pushAlpha(late ? 0.45f : 1.0f);
            if (win) G.softRect(inset(cr, -6), 18, 18, withA(hexc(TIER_COL[tier]), 0.6f + 0.3f * std::sin(time * 8.0f)));
            G.softRect(R(cr.x, cr.y + 6, cr.w, cr.h), 14, 10, hexA(0x000000, 0.6f));
            G.rectBorder(cr, hexc(0x2a2038), hexc(0x0c0812), 14, 3, hexc(TIER_COL[tier]));
            G.sprite(JACKPOT_SPRITES[tier], centered(cr.cx(), cr.cy() - 10, 92, 92));
            TextStyle nt(FONT_UI, 14, hexc(TIER_COL[tier]));
            nt.al(ALIGN_CENTER).sp(1.5f).drop(1.2f);
            G.text(JACKPOT_NAMES[tier], cr.cx(), cr.bottom() - 16, nt);
            G.popAlpha();
        }
    }
    if (jpDoneT >= 0) {
        float s = easeOutBack(clampf(jpDoneT * 2.0f, 0, 1));
        G.pushTransformAround(640, 600, s);
        Rect pr = centered(640, 610, 640, 110);
        G.softRect(pr, 24, 20, hexA(0x000000, 0.7f));
        G.rectBorder(pr, hexc(0x3a2a10), hexc(0x120a02), 24, 3, hexc(TIER_COL[jpTier]));
        char b[64];
        std::snprintf(b, sizeof(b), "JACKPOT %s", JACKPOT_NAMES[jpTier]);
        TextStyle a(FONT_UI, 20, hexc(TIER_COL[jpTier]));
        a.al(ALIGN_CENTER).sp(2).drop(1.5f);
        G.text(b, 640, 578, a);
        TextStyle v(FONT_DISPLAY, 50, hexc(0xffffff));
        v.grad(hexc(0xffffff), hexc(0xffe080)).stroke(3, hexc(0x2a1000)).al(ALIGN_CENTER);
        G.text(lei(jpAmount).c_str(), 640, 626, v);
        G.popTransform();
    }
}

void SlotMachineUI::Impl::drawMoneyTag(Rect cell, double value, float alpha, float scale) {
    std::string s = fmtMoney(value);
    float w = 20.0f + 13.0f * (float)s.size();
    float cx = cell.cx(), cy = cell.y + cell.h * 0.80f;
    cy = cell.cy() + (cy - cell.cy()) * scale;
    Rect pill = centered(cx, cy, w * scale, 34 * scale);
    G.softRect(pill, 17, 8, hexA(0x000000, 0.55f * alpha));
    G.rectGrad(pill, withA(hexc(0xfff2a0), alpha), withA(hexc(0xd08a10), alpha), 17 * scale);
    G.frame(pill, 17 * scale, 2, withA(hexc(0x5a2a00), alpha));
    TextStyle t(FONT_DISPLAY, 24 * scale, withA(hexc(0xffffff), alpha));
    t.stroke(3, withA(hexc(0x4a2000), alpha)).al(ALIGN_CENTER);
    G.text(s.c_str(), cx, cy - 1.5f * scale, t);
}

void SlotMachineUI::Impl::drawCollect() {
    // pozitia pescarului: lipit de dreapta rolelor, deasupra panoului
    Rect win = reelWindow();
    const float W = 200.0f, H = 200.0f;
    float t = collectT >= 0 ? collectT : collectEnd();
    float slide = easeOutCubic(clampf(t / 0.45f, 0, 1));
    float fx = win.x + win.w + 8.0f + (1.0f - slide) * 230.0f;
    Rect fr = R(fx - W * 0.05f, 548.0f - H, W, H);
    Rect fig = R(fr.x + W * 0.055f, fr.y, W * 0.89f, H);     // figura fara marginile transparente
    float mouthX = fig.x + fig.w * 0.50f, mouthY = fig.y + fig.h * 0.70f;
    int fishSym = -1;
    for (size_t i = 0; i < game->symbols.size(); ++i)
        if (game->symbols[i].kind == KIND_MONEY) fishSym = (int)i;
    // 1. pestii care platesc se lumineaza (cat sunt inca pe role)
    float glow = clampf((t - 0.45f) / 0.25f, 0, 1);
    for (int k = 0; k < collectN; ++k) {
        float fly = t - collectFlyStart(k);
        if (fly >= 0) continue;
        Rect cr = cellRect(collectCells[k][0], collectCells[k][1]);
        float p = 0.6f + 0.4f * std::sin(time * 9.0f + k);
        G.softRect(inset(cr, 4), 18, 22, hexA(0xffe060, 0.55f * glow * p));
        G.rect(inset(cr, 6), hexA(0xffffff, 0.10f * glow * p), 14);
        G.frame(inset(cr, 5), 14, 3.0f, hexA(0xffd040, 0.9f * glow));
    }
    // 2. pescarul cu cosul
    G.softRect(R(fr.x + 20, fr.y + H - 16, W - 40, 22), 11, 16, hexA(0x000000, 0.45f * slide));
    G.spriteBig(SPR_FISHER_BASKET, 2, 2, fr, rgba(1, 1, 1, slide));
    // 3. pestii zboara in cos (pe un arc), cu valoarea lor
    double arrived = 0;
    for (int k = 0; k < collectN; ++k) {
        int c = collectCells[k][0], r = collectCells[k][1];
        float fly = (t - collectFlyStart(k)) / 0.55f;
        if (fly >= 1.0f) {
            arrived += result.money[c][r];
            continue;
        }
        if (fly < 0) continue;
        Rect cr = cellRect(c, r);
        float e = fly * fly * (3 - 2 * fly);
        float sx = cr.cx(), sy = cr.cy();
        float cx = (sx + mouthX) * 0.5f, cy = std::min(sy, mouthY) - 150.0f;
        float x = (1 - e) * (1 - e) * sx + 2 * (1 - e) * e * cx + e * e * mouthX;
        float y = (1 - e) * (1 - e) * sy + 2 * (1 - e) * e * cy + e * e * mouthY;
        float s = 1.0f - 0.62f * e;
        Rect fc = centered(x, y, cr.w * s, cr.h * s);
        G.softRect(inset(fc, 8), 16, 16, hexA(0xffe060, 0.35f));
        if (fishSym >= 0) drawSymbol(fishSym, fc, 1.0f, 0, 1.0f);
        drawMoneyTag(fc, result.money[c][r] * bet(), 1.0f, s);
        // stropi la intrarea in cos
        if (fly > 0.85f) {
            for (int q = 0; q < 6; ++q) {
                float a = q * 1.05f + k;
                float d = (fly - 0.85f) * 220.0f;
                G.circle(mouthX + std::cos(a) * d, mouthY - std::fabs(std::sin(a)) * d * 0.8f, 4.0f, hexA(0xbfefff, 1.0f - (fly - 0.85f) / 0.15f));
            }
        }
    }
    // 4. suma stransa, deasupra pescarului
    if (arrived > 0) {
        float lastArrive = collectFlyStart(collectN - 1) + 0.55f;
        bool multPhase = collectMult > 1 && t >= lastArrive + 0.3f;
        double shown = arrived * bet() * (multPhase ? collectMult : 1);
        std::string s = fmtMoney(shown) + " LEI";
        float bump = 1.0f;
        for (int k = 0; k < collectN; ++k) {
            float a = t - (collectFlyStart(k) + 0.55f);
            if (a >= 0 && a < 0.25f) bump = std::max(bump, 1.0f + 0.25f * (1.0f - a / 0.25f));
        }
        Rect bub = centered(std::min(fig.cx(), 1272.0f - 98.0f * bump), fr.y - 34, 196 * bump, 52 * bump);
        G.softRect(bub, 26, 14, hexA(0x000000, 0.55f));
        G.rectGrad(bub, hexc(0xfff2a0), hexc(0xd08a10), 24 * bump);
        G.frame(bub, 24 * bump, 2.5f, hexc(0x5a2a00));
        TextStyle ts(FONT_DISPLAY, 30 * bump, hexc(0xffffff));
        ts.stroke(3, hexc(0x4a2000)).al(ALIGN_CENTER);
        G.textBox(inset(bub, 8), s.c_str(), ts, 12);
        if (collectMult > 1) {
            float ma = clampf((t - lastArrive - 0.3f) / 0.3f, 0, 1);
            if (ma > 0) {
                char m[16];
                std::snprintf(m, sizeof(m), SLOT_U8("× %d PESCARI"), collectMult);
                TextStyle mt(FONT_DISPLAY, 26, hexc(0xfff6c0));
                mt.grad(hexc(0xffffff), hexc(0xffc040)).stroke(3, hexc(0x3a1400)).al(ALIGN_CENTER);
                G.pushAlpha(ma);
                G.text(m, std::min(fig.cx(), 1150.0f), fr.y - 96, mt);
                G.popAlpha();
            }
        }
    }
}

void SlotMachineUI::Impl::autoButton() {
    if (!game) return;
    if (autoPlay) {                       // AUTO pornit: il opreste
        autoPlay = false;
        autoLeft = 0;
        return;
    }
    if (fsActive || spinning || busy() || overlay != OV_NONE) return;
    overlay = OV_AUTO;
    overlayT = 0;
}

void SlotMachineUI::Impl::startAuto(int option) {
    if (option < 0 || option >= AUTO_OPTION_COUNT) return;
    overlay = OV_NONE;
    autoLeft = AUTO_OPTIONS[option];
    autoPlay = true;
    idleT = 1.0f;                         // prima rotire porneste imediat
}

void SlotMachineUI::Impl::drawAutoPanel() {
    const Theme& th = game->theme;
    G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, 0.65f));
    addRegion(R(0, 0, CANVAS_W, CANVAS_H), ACT_BLOCK, 0);
    float s = easeOutBack(clampf(overlayT * 3.0f, 0, 1));
    G.pushTransformAround(640, 314, s);
    Rect d = centered(640, 314, 700, 430);
    panelCard(G, d, th, 24);
    TextStyle tt(FONT_DISPLAY, 40, hexc(0xfff6c0));
    tt.grad(hexc(0xfff6c0), hexc(0xffb020)).stroke(3, hexc(0x2a1000)).drop(3).al(ALIGN_CENTER);
    G.text(SLOT_U8("ROTIRI AUTOMATE"), 640, 140, tt);
    TextStyle bt(FONT_UI, 19, hexc(0xffffff));
    bt.al(ALIGN_CENTER).drop(1.5f);
    G.text(SLOT_U8("Alege câte rotiri să dea aparatul singur:"), 640, 184, bt);
    for (int i = 0; i < AUTO_OPTION_COUNT; ++i) {
        int row = i / 4, col = i % 4;
        int inRow = row == 0 ? 4 : AUTO_OPTION_COUNT - 4;
        float bw = 136, gap = 16;
        float x0 = 640.0f - (inRow * bw + (inRow - 1) * gap) * 0.5f + col * (bw + gap);
        Rect r = R(x0, 212.0f + row * 76.0f, bw, 62);
        char lb[32];
        if (AUTO_OPTIONS[i] < 0) std::snprintf(lb, sizeof(lb), SLOT_U8("PÂNĂ LA OPRIRE"));
        else std::snprintf(lb, sizeof(lb), "%d", AUTO_OPTIONS[i]);
        bool sel = i == autoSel;
        if (sel) G.softRect(inset(r, -4), 18, 14, hexA(0xffd040, 0.55f + 0.2f * std::sin(time * 6.0f)));
        button(r, lb, ACT_AUTO_PICK, i, sel ? BTN_GOLD : BTN_DARK, true, sel, AUTO_OPTIONS[i] < 0 ? 15 : 28);
    }
    G.text(SLOT_U8("Viteza:"), 640, 372, bt);
    static const char* SPEEDS[3] = {"NORMAL", "RAPID", "TURBO"};
    for (int i = 0; i < 3; ++i) {
        Rect r = R(640.0f - 222.0f + i * 152.0f, 394, 140, 48);
        bool sel = i == autoSpeed;
        if (sel) G.softRect(inset(r, -3), 16, 12, hexA(0x40d0ff, 0.5f));
        button(r, SPEEDS[i], ACT_AUTO_SPEED, i, sel ? BTN_GREEN : BTN_DARK, true, sel, 16);
        for (int k = 0; k <= i; ++k)   // sagetile de viteza, sub text
            G.triangle(r.cx() - i * 6.0f + k * 12.0f, r.y + r.h - 9, 9, 1, hexA(0xffffff, sel ? 0.95f : 0.6f));
    }
    TextStyle nt(FONT_UI, 14, hexA(0xffffff, 0.75f));
    nt.al(ALIGN_CENTER);
    G.text(SLOT_U8("AUTO așteaptă la speciale și la jackpot • apasă din nou AUTO ca să-l oprești"), 640, 456, nt);
    button(R(390, 476, 230, 44), SLOT_U8("PORNEȘTE"), ACT_AUTO_GO, 0, BTN_GREEN, true, false, 20);
    button(R(660, 476, 230, 44), SLOT_U8("RENUNȚ"), ACT_AUTO_CANCEL, 0, BTN_RED, true, false, 20);
    G.popTransform();
}

} // namespace slot

