// SlotMachineUI: resurse comune, textura (FBO), intrari, butoane si lobby-ul.
// Fisier UTF-8 (cu BOM).
#include "SlotUIImpl.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace slot {

Renderer2D* gRenderer = nullptr;

namespace {
std::string gError;
int gInstances = 0;

Renderer2D& renderer() {
    if (!gRenderer) gRenderer = new Renderer2D();
    return *gRenderer;
}

bool hasExtension(const char* name) {
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; ++i) {
        const char* e = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, (GLuint)i));
        if (e && std::strcmp(e, name) == 0) return true;
    }
    return false;
}

std::string clockString() {
    std::time_t t = std::time(nullptr);
    std::tm tmv;
    std::memset(&tmv, 0, sizeof(tmv));
#if defined(_MSC_VER)
    localtime_s(&tmv, &t);
#elif defined(_WIN32)
    std::tm* p = std::localtime(&t);
    if (p) tmv = *p;
#else
    localtime_r(&t, &tmv);
#endif
    char b[16];
    std::snprintf(b, sizeof(b), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    return b;
}

const uint32_t JP_COLORS[4] = {0xf0a060, 0xdfe8f8, 0xffd040, 0x8af0ff};
} // namespace

// ------------------------------------------------------------------ utilitare
std::string fmtMoney(double v) {
    long long cents = (long long)std::llround(v * 100.0);
    bool neg = cents < 0;
    if (neg) cents = -cents;
    long long whole = cents / 100;
    int frac = (int)(cents % 100);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", whole);
    std::string w = buf, out;
    int cnt = 0;
    for (int i = (int)w.size() - 1; i >= 0; --i) {
        out.insert(out.begin(), w[(size_t)i]);
        if (++cnt % 3 == 0 && i > 0) out.insert(out.begin(), '.');
    }
    std::snprintf(buf, sizeof(buf), ",%02d", frac);
    out += buf;
    return neg ? "-" + out : out;
}

float clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
float easeOutCubic(float t) {
    t = clampf(t, 0, 1);
    float u = 1 - t;
    return 1 - u * u * u;
}
float easeInOut(float t) {
    t = clampf(t, 0, 1);
    return t * t * (3 - 2 * t);
}
float easeOutBack(float t) {
    t = clampf(t, 0, 1);
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
Color hexA(uint32_t rgb, float a) {
    return hexc(rgb, a);
}

// ------------------------------------------------------------------ resurse comune
bool SlotMachineUI::initShared(const std::string& assetDir) {
    Renderer2D& r = renderer();
    if (r.ready()) return true;
    std::string err;
    if (!r.init(assetDir, &err)) {
        gError = err;
        std::fprintf(stderr, "[SlotUI] Eroare la initializare: %s\n", err.c_str());
        return false;
    }
    catalog();
    jackpots().init();
    std::string sdir = assetDir;
    if (!sdir.empty() && sdir[sdir.size() - 1] != '/' && sdir[sdir.size() - 1] != '\\') sdir += "/";
    audio::init(sdir + "sounds/");   // fara sunete aparatele merg, doar ca sunt silentioase
    gError.clear();
    return true;
}

void SlotMachineUI::shutdownShared() {
    audio::shutdown();
    if (gRenderer) {
        gRenderer->shutdown();
        delete gRenderer;
        gRenderer = nullptr;
    }
}

const std::string& SlotMachineUI::lastError() {
    return gError;
}

// ------------------------------------------------------------------ API public
SlotMachineUI::SlotMachineUI(int texWidth, int texHeight, Wallet* wallet) : p_(new Impl(texWidth, texHeight, wallet)) {}
SlotMachineUI::~SlotMachineUI() { delete p_; }
void SlotMachineUI::update(float dt) { p_->update(dt); }
void SlotMachineUI::render() { p_->render(); }
GLuint SlotMachineUI::texture() const { return p_->tex; }
void SlotMachineUI::renderTop() { p_->renderTop(); }
GLuint SlotMachineUI::topTexture() const { return p_->topTex; }
int SlotMachineUI::width() const { return p_->w; }
int SlotMachineUI::height() const { return p_->h; }

void SlotMachineUI::mouseMove(float u, float v) {
    p_->toVirtual(u, v, p_->mouseX, p_->mouseY);
    p_->mouseInside = u >= 0 && u <= 1 && v >= 0 && v <= 1;
    p_->keyboardFocus = false;
}
void SlotMachineUI::mouseDown(float u, float v) {
    mouseMove(u, v);
    p_->mouseHeld = true;
    if (!p_->mouseInside) return;
    InputEvent e = {0, p_->mouseX, p_->mouseY, 0};
    p_->events.push_back(e);
}
void SlotMachineUI::mouseUp(float u, float v) {
    mouseMove(u, v);
    p_->mouseHeld = false;
}
void SlotMachineUI::mouseLeave() {
    p_->mouseInside = false;
    p_->mouseHeld = false;
}
void SlotMachineUI::scroll(float dy) {
    InputEvent e = {2, dy, 0, 0};
    p_->events.push_back(e);
}
void SlotMachineUI::keyPress(int key) {
    InputEvent e = {1, 0, 0, key};
    p_->events.push_back(e);
}
bool SlotMachineUI::inLobby() const { return p_->screen == 0; }
bool SlotMachineUI::isBusy() const { return p_->busy(); }
void SlotMachineUI::goToLobby() {
    if (p_->screen == 1 && !p_->busy()) p_->leaveGame();
}
const char* SlotMachineUI::currentGameName() const { return p_->game ? p_->game->name.c_str() : ""; }
bool SlotMachineUI::celebrating() const { return p_->celebrating(); }
double SlotMachineUI::balance() const { return p_->wallet->balance; }
void SlotMachineUI::addCredit(double lei) {
    p_->wallet->balance += lei;
}
Wallet& SlotMachineUI::wallet() { return *p_->wallet; }
void SlotMachineUI::setVolume(float v) { audio::setOwnerVolume(p_->audioOwner, v); }
void SlotMachineUI::setMasterVolume(float v) { audio::setMasterVolume(v); }
void SlotMachineUI::setMuted(bool m) { audio::setMuted(m); }
bool SlotMachineUI::muted() { return audio::muted(); }

// ------------------------------------------------------------------ Impl
SlotMachineUI::Impl::Impl(int width, int height, Wallet* wl)
    : w(width > 16 ? width : 16), h(height > 16 ? height : 16), fbo(0), tex(0), topFbo(0), topTex(0), mipLevels(1),
      ownWallet(500.0), wallet(wl ? wl : &ownWallet), time(0), mouseX(-100), mouseY(-100),
      mouseInside(false), mouseHeld(false), keyboardFocus(false), drawingOverlay(false), hoverSlot(-1),
      creditShown(0), toastT(0), G(renderer()), audioOwner(audio::newOwner()), spinVoice(0), anticVoice(0), countVoice(0), screen(0), tab(0), selected(0), pageAnimT(1), pageAnimDir(1),
      pageAnimFrom(0), tabAnimT(1), transT(1), transTo(0), transGame(-1), transTile(-1), gameIndex(-1),
      game(nullptr), betStep(3), spinning(false), spinT(0), haveResult(false), presenting(false), presentT(0),
      pendingWin(0), winShown(0), winTarget(0), winFrom(0), winCountT(1), winCountDur(1), expandT(-1), collectT(-1), fishCollected(false), collectN(0), collectMult(1),
      expandReels(0), bigWinT(-1), bigWinAmount(0), autoPlay(false), autoLeft(0), autoSel(3), autoSpeed(0), idleT(0), overlay(OV_NONE), overlayT(0),
      fsActive(false), fsLeft(0), fsPlayed(0), fsTotal(0), fsWon(0), special(-1), fsPendingAward(0),
      fsRetriggerT(-1), buying(false), gambleAmount(0), gambleSteps(0), gambleHistCount(0), gambleCard(-1),
      gambleRank(0), gambleFlipT(-1), gambleResult(0), gambleResultT(0), infoPage(0), jpTierPending(-1),
      jpTier(-1), jpAmount(0), jpSeqLen(0), jpPicks(0), jpSel(0), jpDoneT(-1), jpPaid(false) {
    ++gInstances;
    for (int i = 0; i < 8; ++i) page[i] = 0;
    for (int i = 0; i < 64; ++i) tileHover[i] = 0;
    for (int i = 0; i < 6; ++i) gambleHistory[i] = 0;
    for (int i = 0; i < 12; ++i) jpCards[i] = jpSeq[i] = -1;
    std::memset(reels, 0, sizeof(reels));
    creditShown = wallet->balance;
    vscale = std::min(w / CANVAS_W, h / CANVAS_H);
    voffX = (w - CANVAS_W * vscale) * 0.5f;
    voffY = (h - CANVAS_H * vscale) * 0.5f;
    jackpots().init();

    GLint prevTex = 0, prevFbo = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo);
    int m = std::max(w, h);
    while (m > 1) {
        m >>= 1;
        ++mipLevels;
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipLevels - 1);
    glGenerateMipmap(GL_TEXTURE_2D);
    if (hasExtension("GL_EXT_texture_filter_anisotropic") || hasExtension("GL_ARB_texture_filter_anisotropic")) {
        GLfloat maxA = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxA);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, maxA));
    }
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        gError = "FBO incomplet pentru ecranul aparatului";
        std::fprintf(stderr, "[SlotUI] %s\n", gError.c_str());
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);
}

SlotMachineUI::Impl::~Impl() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (tex) glDeleteTextures(1, &tex);
    if (topFbo) glDeleteFramebuffers(1, &topFbo);
    if (topTex) glDeleteTextures(1, &topTex);
    audio::stopOwner(audioOwner);
    --gInstances;
}

void SlotMachineUI::Impl::toVirtual(float u, float v, float& x, float& y) const {
    float px = u * w, py = (1.0f - v) * h;
    x = (px - voffX) / vscale;
    y = (py - voffY) / vscale;
}

bool SlotMachineUI::Impl::busy() const {
    if (screen != 1) return transT < 1.0f;
    return spinning || fsActive || overlay == OV_FS_INTRO || overlay == OV_FS_OUTRO || overlay == OV_GAMBLE || overlay == OV_AUTO ||
           overlay == OV_JACKPOT || overlay == OV_BUY || bigWinT >= 0 || expandT >= 0 || collectT >= 0 || fsPendingAward > 0 ||
           jpTierPending >= 0 || transT < 1.0f;
}

bool SlotMachineUI::Impl::celebrating() const {
    return bigWinT >= 0 || overlay == OV_JACKPOT || overlay == OV_FS_INTRO || overlay == OV_FS_OUTRO;
}

// ------------------------------------------------------------------ cadru
void SlotMachineUI::Impl::update(float dt) {
    if (dt < 0) dt = 0;
    if (dt > 0.1f) dt = 0.1f;
    time += dt;
    std::vector<InputEvent> ev;
    ev.swap(events);
    for (size_t i = 0; i < ev.size(); ++i) handleEvent(ev[i]);

    jackpots().tick(dt / (float)std::max(1, gInstances));

    double bal = wallet->balance;
    if (bal < creditShown) creditShown = bal;
    else if (bal > creditShown) {
        double step = std::max(0.01, (bal - creditShown) * dt * 5.0 + dt * 3.0);
        creditShown = std::min(bal, creditShown + step);
    }
    if (pageAnimT < 1) pageAnimT = std::min(1.0f, pageAnimT + dt / 0.38f);
    if (tabAnimT < 1) tabAnimT = std::min(1.0f, tabAnimT + dt / 0.30f);
    for (int i = 0; i < 64; ++i) {
        float target = (i == hoverSlot) ? 1.0f : 0.0f;
        tileHover[i] += (target - tileHover[i]) * std::min(1.0f, dt * 12.0f);
    }
    if (transT < 1.0f) {
        float prev = transT;
        transT = std::min(1.0f, transT + dt / 0.55f);
        if (prev < 0.5f && transT >= 0.5f) {
            if (transTo == 1) {
                enterGame(transGame);
                screen = 1;
            } else {
                screen = 0;
                game = nullptr;
                gameIndex = -1;
            }
        }
    }
    if (toastT > 0) toastT -= dt;
    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.vy += 1500.0f * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.rot += p.spin * dt;
        p.life -= dt;
        if (p.life <= 0 || p.y > 800) {
            particles[i] = particles.back();
            particles.pop_back();
        } else {
            ++i;
        }
    }
    if (screen == 1 && game) updateGame(dt);
}

void SlotMachineUI::Impl::render() {
    GLStateBackup st;
    st.save();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    regions.clear();
    if (!G.ready()) {
        // resursele nu sunt incarcate: ecran rosu-inchis ca semnal (vezi lastError())
        glClearColor(0.25f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        G.begin(w, h, time);
        drawingOverlay = false;
        if (screen == 0) drawLobby();
        else drawGame();
        drawParticles();
        drawToast();
        if (transT < 1.0f) {
            float a = transT < 0.5f ? transT * 2.0f : (1.0f - transT) * 2.0f;
            G.rect(R(0, 0, CANVAS_W, CANVAS_H), hexA(0x000000, easeInOut(a)));
        }
        G.end();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)st.fbo);   // textura nu mai e tinta de randare
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    st.restore();
}

void SlotMachineUI::Impl::renderTop() {
    GLStateBackup st;
    st.save();
    if (!topTex) {
        glGenTextures(1, &topTex);
        glBindTexture(GL_TEXTURE_2D, topTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipLevels - 1);
        glGenerateMipmap(GL_TEXTURE_2D);
        glGenFramebuffers(1, &topFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, topFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, topTex, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, topFbo);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (G.ready()) {
        G.begin(w, h, time);
        drawTopScreen();
        G.end();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)st.fbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, topTex);
    glGenerateMipmap(GL_TEXTURE_2D);
    st.restore();
}

void SlotMachineUI::Impl::drawTopScreen() {
    const Rect full = R(0, 0, CANVAS_W, CANVAS_H);
    if (screen != 1 || !game) {
        // ---- lobby: cele 4 jackpot-uri, mari
        G.rectGrad(full, hexc(0x2a0a46), hexc(0x07020f));
        G.rays(centered(640, 380, 1800, 1800), hexA(0xb060ff, 0.10f), hexA(0xb060ff, 0.0f), 18, time * 0.08f);
        for (int i = 0; i < 30; ++i) {
            float tw = 0.5f + 0.5f * std::sin(time * (1.3f + 0.13f * i) + i * 2.3f);
            float x = std::fmod(i * 397.0f, 1280.0f), y = std::fmod(i * 211.0f, 700.0f);
            G.pushAlpha(tw * 0.7f);
            G.sprite(SPR_SPARKLE, centered(x, y, 18 + (i % 5) * 6.0f, 18 + (i % 5) * 6.0f));
            G.popAlpha();
        }
        TextStyle t(FONT_DISPLAY, 120, hexc(0xfff2a0));
        t.grad(hexc(0xfff6c0), hexc(0xffa020)).stroke(6, hexc(0x3a1000)).drop(5).halo(18, hexA(0xffa020, 0.6f + 0.2f * std::sin(time * 3.0f))).al(ALIGN_CENTER);
        G.text("JACKPOT", 640, 70, t);
        TextStyle s(FONT_UI, 26, hexc(0xe8d8ff));
        s.al(ALIGN_CENTER).sp(4).drop(2);
        G.text(SLOT_U8("MISTERIOS  •  4 NIVELURI  •  COMUN TUTUROR APARATELOR"), 640, 216, s);
        for (int j = 0; j < 4; ++j) {
            Rect r = R(j % 2 ? 660.0f : 100.0f, j < 2 ? 290.0f : 470.0f, 520, 150);
            float pulse = 0.5f + 0.5f * std::sin(time * 2.0f + j * 1.3f);
            Color c = hexc(JP_COLORS[3 - j]);
            G.softRect(inset(r, -6), 26, 22, withA(c, 0.25f + 0.25f * pulse));
            G.rectBorder(r, hexc(0x241a36), hexc(0x0b0812), 24, 3.0f, withA(c, 0.95f));
            G.sprite(JACKPOT_SPRITES[3 - j], R(r.x + 12, r.y + 15, 120, 120));
            TextStyle a(FONT_UI, 30, c);
            a.sp(3.0f);
            G.textBox(R(r.x + 150, r.y + 16, r.w - 170, 40), JACKPOT_NAMES[3 - j], a);
            TextStyle b(FONT_DISPLAY, 56, hexc(0xffffff));
            b.grad(hexc(0xffffff), brighten(c, 0.95f)).drop(3).al(ALIGN_RIGHT);
            std::string v = fmtMoney(jackpots().value[3 - j]) + " LEI";
            G.textBox(R(r.x + 150, r.y + 60, r.w - 175, 74), v.c_str(), b, 24);
        }
        return;
    }
    const Theme& th = game->theme;
    drawBackground(th, full, 1.0f);
    TextStyle tt(th.titleFont, th.titleFont == FONT_SERIF ? 70.0f : 66.0f, hexc(th.titleTop));
    tt.grad(hexc(th.titleTop), hexc(th.titleBottom)).stroke(4.0f, hexc(th.titleOutline)).drop(4.0f).halo(12, withA(hexc(th.accent), 0.6f)).al(ALIGN_CENTER);
    int n = (int)game->symbols.size();
    if (game->tab == 0) {
        // ---- clasice: tabelul de plati la pariul curent
        G.textBox(R(140, 14, 1000, 84), game->name.c_str(), tt, 30);
        char hb[128];
        std::snprintf(hb, sizeof(hb), SLOT_U8("CÂȘTIGURI LA PARIUL DE %s LEI  •  %d LINII"), fmtMoney(bet()).c_str(), game->lines);
        TextStyle h(FONT_UI, 22, hexc(0xffe080));
        h.al(ALIGN_CENTER).sp(2).drop(2);
        G.text(hb, 640, 104, h);
        int cols = n > 8 ? 5 : 4;
        int rows = (n + cols - 1) / cols;
        float cw = 1220.0f / cols, chh = std::min(270.0f, 570.0f / rows);
        for (int i = 0; i < n; ++i) {
            int idx = n - 1 - i;     // simbolurile mari primele
            const SymbolDef& sd = game->symbols[(size_t)idx];
            int c = i % cols, r = i / cols;
            int inRow = std::min(cols, n - r * cols);
            float x0 = 640.0f - inRow * cw * 0.5f + c * cw;
            Rect cell = R(x0 + 6, 142.0f + r * chh, cw - 12, chh - 12);
            G.rect(cell, hexA(0x000000, 0.38f), 18);
            G.frame(cell, 18, 1.5f, withA(hexc(th.frame), 0.6f));
            float sz = std::min(cell.h - 20, cell.w * (cols > 4 ? 0.38f : 0.44f));
            drawSymbol(idx, R(cell.x + 8, cell.y + (cell.h - sz) * 0.5f, sz, sz), 1, 0, 1);
            float tx = cell.x + sz + 16, ty = cell.y + 14;
            {
                int cnt = 0;
                for (int k = 5; k >= 2; --k)
                    if (k <= game->reels && sd.pay[k] > 0) ++cnt;
                bool lab = sd.kind == KIND_WILD || sd.kind == KIND_SCATTER || sd.kind == KIND_BOOK;
                float blockH = (lab ? 24.0f : 0.0f) + (cnt ? cnt * std::min(40.0f, (cell.h - 40) / cnt) : 50.0f);
                ty = std::max(cell.y + 12, cell.y + (cell.h - blockH) * 0.5f);
            }
            if (sd.kind == KIND_WILD || sd.kind == KIND_SCATTER || sd.kind == KIND_BOOK) {
                TextStyle wt(FONT_UI, 17, hexc(0x9af0ff));
                const char* lab = sd.kind == KIND_SCATTER ? "SCATTER" : (game->expandingWild ? "WILD EXTINS" : "WILD");
                G.text(lab, tx, ty, wt);
                ty += 24;
            }
            int shown = 0;
            for (int k = 5; k >= 2; --k) {
                if (k > game->reels || sd.pay[k] <= 0) continue;
                ++shown;
            }
            float lh = std::min(40.0f, (cell.y + cell.h - 10 - ty) / std::max(1, shown));
            for (int k = 5; k >= 2; --k) {
                if (k > game->reels || sd.pay[k] <= 0) continue;
                double v = sd.pay[k] * game->payScale * ((sd.kind == KIND_SCATTER || sd.kind == KIND_BOOK) ? bet() : lineBet());
                char a[16];
                std::snprintf(a, sizeof(a), SLOT_U8("%d×"), k);
                TextStyle pn(FONT_DISPLAY, std::min(30.0f, lh * 0.8f), hexc(0xffd060));
                pn.drop(1.5f);
                TextStyle pt(FONT_DISPLAY, std::min(30.0f, lh * 0.8f), hexc(0xffffff));
                pt.drop(1.5f);
                G.textBox(R(tx, ty, 44, lh), a, pn, 12);
                G.textBox(R(tx + 46, ty, cell.x + cell.w - tx - 54, lh), fmtMoney(v).c_str(), pt, 12);
                ty += lh;
            }
            if (shown == 0) {
                TextStyle wt(FONT_UI, 17, hexA(0xffffff, 0.85f));
                G.text(SLOT_U8("înlocuiește"), tx, ty + 6, wt);
                G.text("simbolurile", tx, ty + 30, wt);
            }
        }
        return;
    }
    // ---- speciale: imaginea jocului
    G.rays(centered(640, 330, 1500, 1500), withA(hexc(th.accent), 0.22f), withA(hexc(th.accent), 0.0f), 14, time * 0.25f);
    G.radial(centered(640, 330, 760, 760), withA(hexc(th.accent), 0.45f), withA(hexc(th.accent), 0.0f));
    // cele mai valoroase simboluri, de o parte si de alta
    std::vector<int> best;
    for (int i = n - 1; i >= 0 && (int)best.size() < 4; --i) {
        int k = game->symbols[(size_t)i].kind;
        if (k == KIND_NORMAL || k == KIND_WILD || k == KIND_MONEY) best.push_back(i);
    }
    for (size_t i = 0; i < best.size(); ++i) {
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        float x = 640.0f + side * (i < 2 ? 330.0f : 500.0f);
        float y = (i < 2 ? 300.0f : 380.0f) + 12.0f * std::sin(time * 1.6f + (float)i);
        float s = i < 2 ? 210.0f : 170.0f;
        G.pushAlpha(0.95f);
        drawSymbol(best[i], centered(x, y, s, s), 1.0f, 0, 1.0f);
        G.popAlpha();
    }
    float bob = 1.0f + 0.03f * std::sin(time * 2.2f);
    G.softRect(centered(640, 320, 330, 330), 160, 40, hexA(0x000000, 0.35f));
    G.sprite(game->thumbSprite, centered(640, 316, 330 * bob, 330 * bob));
    G.textBox(R(100, 12, 1080, 96), game->name.c_str(), tt, 30);
    const char* feat = "";
    char fb[160];
    if (game->fsType == FS_EXPANDING) {
        std::snprintf(fb, sizeof(fb), SLOT_U8("%d ROTIRI SPECIALE CU SIMBOL CARE SE EXTINDE"), game->fsCount);
        feat = fb;
    } else if (game->fsType == FS_MULTIPLIER) {
        std::snprintf(fb, sizeof(fb), SLOT_U8("%d ROTIRI SPECIALE CU CÂȘTIGURI ×%d"), game->fsCount, game->fsMultiplier);
        feat = fb;
    } else if (game->fsType == FS_COLLECT) {
        std::snprintf(fb, sizeof(fb), SLOT_U8("%d ROTIRI SPECIALE  •  PESCARUL STRÂNGE PEȘTII"), game->fsCount);
        feat = fb;
    }
    G.rect(R(0, 560, CANVAS_W, 160), hexA(0x000000, 0.45f));
    TextStyle ft(FONT_DISPLAY, 40, hexc(0xfff2b0));
    ft.grad(hexc(0xffffff), hexc(0xffc040)).stroke(3, hexc(0x2a1000)).drop(3).al(ALIGN_CENTER);
    G.textBox(R(60, 580, 1160, 56), feat, ft, 22);
    if (game->buyCost > 0) {
        char bb[96];
        std::snprintf(bb, sizeof(bb), SLOT_U8("SPECIALA SE POATE CUMPĂRA  •  %d × PARIUL"), game->buyCost);
        TextStyle bt(FONT_UI, 24, hexc(0xd8e8ff));
        bt.al(ALIGN_CENTER).sp(2);
        G.text(bb, 640, 652, bt);
    }
}

// ------------------------------------------------------------------ intrari
void SlotMachineUI::Impl::handleEvent(const InputEvent& e) {
    if (e.type == 1) {
        if (transT < 1.0f) return;
        handleKey(e.key);
        return;
    }
    if (e.type == 2) {
        if (transT < 1.0f) return;
        if (screen == 0) {
            if (e.x < 0) doAction(ACT_PAGE_NEXT, 0);
            else if (e.x > 0) doAction(ACT_PAGE_PREV, 0);
        } else if (overlay == OV_INFO) {
            doAction(e.x < 0 ? ACT_INFO_NEXT : ACT_INFO_PREV, 0);
        }
        return;
    }
    if (transT < 1.0f) return;
    for (int i = (int)regions.size() - 1; i >= 0; --i) {
        const Region& r = regions[(size_t)i];
        if (r.r.contains(e.x, e.y)) {
            doAction(r.action, r.param);
            return;
        }
    }
}

void SlotMachineUI::Impl::handleKey(int key) {
    if (key == KEY_V) {
        audio::setMuted(!audio::muted());
        showToast(audio::muted() ? "SUNET OPRIT" : "SUNET PORNIT");
        return;
    }
    if (screen == 0) lobbyKey(key);
    else gameKey(key);
}

void SlotMachineUI::Impl::doAction(int action, int param) {
    if (action != ACT_NONE && action != ACT_BLOCK && action != ACT_START && action != ACT_JP_PICK &&
        action != ACT_GAMBLE_RED && action != ACT_GAMBLE_BLACK && action != ACT_PAGE && action != ACT_PAGE_PREV &&
        action != ACT_PAGE_NEXT)
        snd(audio::SND_CLICK);
    switch (action) {
        case ACT_NONE:
        case ACT_BLOCK: break;
        case ACT_TAB: setTab(param); break;
        case ACT_PAGE: setPage(param, param > page[tab] ? 1 : -1); break;
        case ACT_PAGE_PREV: setPage(page[tab] - 1, -1); break;
        case ACT_PAGE_NEXT: setPage(page[tab] + 1, 1); break;
        case ACT_OPEN_GAME: {
            std::vector<int> list = gamesInTab(tab);
            int slotIdx = -1;
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i] == param) slotIdx = (int)i - page[tab] * 10;
            openGame(param, slotIdx);
            break;
        }
        case ACT_START:
            if (overlay == OV_NONE) {
                if (spinning) quickStop();
                else if (bigWinT >= 0) bigWinT = std::max(bigWinT, 3.2f);
                else startSpin();
            }
            break;
        case ACT_BET_MINUS: changeBet(-1); break;
        case ACT_BET_PLUS: changeBet(1); break;
        case ACT_AUTO: autoButton(); break;
        case ACT_AUTO_PICK:
            autoSel = param;
            startAuto(param);
            break;
        case ACT_AUTO_GO: startAuto(autoSel); break;
        case ACT_AUTO_SPEED: autoSpeed = std::max(0, std::min(2, param)); break;
        case ACT_AUTO_CANCEL: overlay = OV_NONE; break;
        case ACT_GAMBLE: startGamble(); break;
        case ACT_INFO:
            if (overlay == OV_NONE && !spinning) {
                overlay = OV_INFO;
                overlayT = 0;
                infoPage = 0;
            } else if (overlay == OV_INFO) {
                overlay = OV_NONE;
            }
            break;
        case ACT_MENU:
            if (!busy()) leaveGame();
            break;
        case ACT_BUY:
            if (!busy() && game && game->buyCost > 0) {
                collect();
                overlay = OV_BUY;
                overlayT = 0;
            }
            break;
        case ACT_GAMBLE_RED: gamble(true); break;
        case ACT_GAMBLE_BLACK: gamble(false); break;
        case ACT_GAMBLE_COLLECT: finishGamble(); break;
        case ACT_INFO_PREV: infoPage = std::max(0, infoPage - 1); break;
        case ACT_INFO_NEXT: infoPage = std::min(1, infoPage + 1); break;
        case ACT_INFO_CLOSE: overlay = OV_NONE; break;
        case ACT_BUY_YES: startBuy(); break;
        case ACT_BUY_NO: overlay = OV_NONE; break;
        case ACT_JP_PICK: pickJackpot(param); break;
        case ACT_CONTINUE:
            if (overlay == OV_FS_INTRO && overlayT > 0.6f) {
                overlayT = std::max(overlayT, 2.6f);
                if (overlayT >= 2.6f) startFreeSpins();
            } else if (overlay == OV_FS_OUTRO && overlayT > 0.5f) {
                endFreeSpins();
            } else if (overlay == OV_JACKPOT && jpDoneT > 0.8f) {
                overlay = OV_NONE;
            } else if (bigWinT >= 0) {
                if (bigWinT < 3.2f) bigWinT = 3.2f;
                else bigWinT = -1;
            }
            break;
        default: break;
    }
}

// ------------------------------------------------------------------ elemente comune
void SlotMachineUI::Impl::addRegion(Rect r, int action, int param) {
    Region g = {r, action, param};
    regions.push_back(g);
}

bool SlotMachineUI::Impl::hovered(Rect r) const {
    if (!mouseInside || keyboardFocus || transT < 1.0f) return false;
    if (overlay != OV_NONE && !drawingOverlay) return false;
    return r.contains(mouseX, mouseY);
}

bool SlotMachineUI::Impl::button(Rect r, const char* label, int action, int param, int style, bool enabled, bool active, float fs) {
    bool hov = enabled && hovered(r);
    bool press = hov && mouseHeld;
    if (enabled) addRegion(r, action, param);
    Color top, bot, border, txt;
    float rad = 10.0f;
    switch (style) {
        case BTN_GOLD: top = hexc(0xffe68a); bot = hexc(0xd08a12); border = hexc(0xfff2c0); txt = hexc(0x2a1400); rad = r.h * 0.5f; break;
        case BTN_GREEN: top = hexc(0x5ae87a); bot = hexc(0x0f7a28); border = hexc(0xb0ffc0); txt = hexc(0xffffff); break;
        case BTN_RED: top = hexc(0xff6a6a); bot = hexc(0x9a0a16); border = hexc(0xffb0b0); txt = hexc(0xffffff); break;
        case BTN_BLACK: top = hexc(0x50505e); bot = hexc(0x08080c); border = hexc(0xb0b0c0); txt = hexc(0xffffff); break;
        case BTN_GLASS: top = hexA(0x3c2c66, 0.92f); bot = hexA(0x170e2c, 0.92f); border = hexA(0xa892e0, 0.75f); txt = hexc(0xeee8ff); rad = r.h * 0.5f; break;
        default: top = hexc(0x3c3252); bot = hexc(0x16111e); border = hexA(0xd0a848, 0.85f); txt = hexc(0xffffff); break;
    }
    float k = press ? 0.85f : (hov ? 1.18f : 1.0f);
    top = brighten(top, k);
    bot = brighten(bot, k);
    G.pushAlpha(enabled ? 1.0f : 0.38f);
    Rect rr = press ? R(r.x, r.y + 1.5f, r.w, r.h) : r;
    if (active || hov) G.softRect(rr, rad, 12, style == BTN_GOLD || active ? hexA(0xffc040, active ? 0.55f : 0.35f) : hexA(0xa080ff, 0.35f));
    G.softRect(R(rr.x, rr.y + 3, rr.w, rr.h), rad, 5, hexA(0x000000, 0.45f));
    G.rectBorder(rr, top, bot, rad, 1.6f, border);
    G.rectGrad(R(rr.x + 3, rr.y + 2, rr.w - 6, rr.h * 0.46f), hexA(0xffffff, 0.24f), hexA(0xffffff, 0.03f), std::max(0.0f, rad - 2));
    if (label && *label) {
        TextStyle ts(FONT_UI, fs > 0 ? fs : r.h * 0.42f, txt);
        ts.al(ALIGN_CENTER);
        if (style != BTN_GOLD) ts.drop(1.5f, hexA(0x000000, 0.6f));
        G.textBox(inset(rr, 6), label, ts);
    }
    G.popAlpha();
    return hov;
}

void SlotMachineUI::Impl::drawJackpotBadge(Rect r, int tier, float pulse) {
    Color c = hexc(JP_COLORS[tier]);
    G.softRect(R(r.x, r.y + 3, r.w, r.h), 12, 6, hexA(0x000000, 0.5f));
    if (pulse > 0) G.softRect(inset(r, -4), 14, 12, withA(c, 0.6f * pulse));
    G.rectBorder(r, hexc(0x241a36), hexc(0x0b0812), 12, 1.6f, withA(c, 0.85f));
    G.rectGrad(R(r.x + 3, r.y + 2, r.w - 6, r.h * 0.45f), hexA(0xffffff, 0.12f), hexA(0xffffff, 0.0f), 10);
    float s = r.h - 8;
    G.sprite(JACKPOT_SPRITES[tier], R(r.x + 2, r.y + 4, s, s));
    TextStyle a(FONT_UI, 12, c);
    a.sp(1.0f);
    G.textBox(R(r.x + s + 4, r.y + 5, r.w - s - 10, 15), JACKPOT_NAMES[tier], a);
    TextStyle b(FONT_DISPLAY, 19, hexc(0xffffff));
    b.grad(hexc(0xffffff), brighten(c, 0.95f)).drop(1.5f);
    G.textBox(R(r.x + s + 4, r.y + 21, r.w - s - 10, r.h - 24), fmtMoney(jackpots().value[tier]).c_str(), b, 10);
}

void SlotMachineUI::Impl::drawCreditBox(Rect r, const char* label, double value, bool highlight) {
    G.softRect(R(r.x, r.y + 3, r.w, r.h), 10, 6, hexA(0x000000, 0.5f));
    if (highlight) G.softRect(inset(r, -3), 12, 12, hexA(0xffd040, 0.35f + 0.25f * std::sin(time * 8.0f)));
    G.rectBorder(r, hexc(0x110d1a), hexc(0x050409), 10, 1.6f, hexA(0xd0a848, 0.85f));
    G.rectGrad(R(r.x + 3, r.y + 2, r.w - 6, r.h * 0.4f), hexA(0xffffff, 0.08f), hexA(0xffffff, 0.0f), 8);
    TextStyle a(FONT_UI, 13, hexc(0xffd060));
    a.sp(1.2f);
    G.textBox(R(r.x + 12, r.y + 6, r.w - 24, 16), label, a);
    TextStyle b(FONT_DISPLAY, 25, hexc(0xffffff));
    b.al(ALIGN_RIGHT).drop(1.5f);
    std::string v = fmtMoney(value) + " LEI";
    G.textBox(R(r.x + 12, r.y + 22, r.w - 24, r.h - 26), v.c_str(), b, 12);
}

void SlotMachineUI::Impl::spawnCoins(int n, float x, float y, float spread) {
    for (int i = 0; i < n && particles.size() < 220; ++i) {
        Particle p;
        float a = (float)std::rand() / (float)RAND_MAX, b = (float)std::rand() / (float)RAND_MAX, c = (float)std::rand() / (float)RAND_MAX;
        p.x = x + (a - 0.5f) * spread;
        p.y = y;
        p.vx = (b - 0.5f) * 900.0f;
        p.vy = -650.0f - c * 650.0f;
        p.rot = a * 6.28f;
        p.spin = 5.0f + b * 9.0f;
        p.life = 2.2f + c * 1.0f;
        p.size = 34.0f + b * 30.0f;
        p.sprite = SPR_COIN;
        particles.push_back(p);
    }
}

void SlotMachineUI::Impl::drawParticles() {
    for (size_t i = 0; i < particles.size(); ++i) {
        const Particle& p = particles[i];
        float sx = std::fabs(std::cos(p.rot));
        G.pushAlpha(clampf(p.life * 2.0f, 0, 1));
        G.sprite(p.sprite, centered(p.x, p.y, p.size * std::max(0.15f, sx), p.size));
        G.popAlpha();
    }
}

void SlotMachineUI::Impl::showToast(const std::string& s) {
    toast = s;
    toastT = 2.6f;
    if (s.find("INSUFICIENT") != std::string::npos) snd(audio::SND_BUZZ);
}

void SlotMachineUI::Impl::drawToast() {
    if (toastT <= 0 || toast.empty()) return;
    float a = clampf(toastT * 3.0f, 0, 1);
    G.pushAlpha(a);
    float tw = G.textWidth(toast.c_str(), FONT_UI, 22) + 60;
    Rect r = centered(640, screen == 0 ? 640 : 470, tw, 50);
    G.softRect(r, 25, 16, hexA(0x000000, 0.6f));
    G.rectBorder(r, hexc(0xc81e30), hexc(0x6a0010), 25, 2, hexc(0xffb0b0));
    TextStyle t(FONT_UI, 22, hexc(0xffffff));
    t.al(ALIGN_CENTER).drop(1.5f);
    G.textBox(r, toast.c_str(), t);
    G.popAlpha();
}

// ------------------------------------------------------------------ lobby
int SlotMachineUI::Impl::tabCount() const {
    return (int)catalog().tabNames.size();
}

int SlotMachineUI::Impl::pageCount(int t) const {
    int n = (int)gamesInTab(t).size();
    return std::max(1, (n + 9) / 10);
}

void SlotMachineUI::Impl::setPage(int p, int dir) {
    if (p < 0 || p >= pageCount(tab) || p == page[tab]) return;
    pageAnimFrom = page[tab];
    page[tab] = p;
    pageAnimDir = dir >= 0 ? 1 : -1;
    pageAnimT = 0;
    snd(audio::SND_PAGE);
    int n = (int)gamesInTab(tab).size() - p * 10;
    if (selected >= n) selected = std::max(0, n - 1);
}

void SlotMachineUI::Impl::setTab(int t) {
    if (t < 0 || t >= tabCount() || t == tab) return;
    tab = t;
    tabAnimT = 0;
    snd(audio::SND_CLICK);
    pageAnimT = 1;
    selected = 0;
}

void SlotMachineUI::Impl::openGame(int catalogIndex, int tileSlot) {
    if (transT < 1.0f || catalogIndex < 0 || catalogIndex >= (int)catalog().games.size()) return;
    transTo = 1;
    transGame = catalogIndex;
    transTile = tileSlot;
    transT = 0;
}

void SlotMachineUI::Impl::lobbyKey(int key) {
    std::vector<int> list = gamesInTab(tab);
    int pages = pageCount(tab);
    int p = page[tab];
    int n = std::min(10, (int)list.size() - p * 10);
    if (!keyboardFocus && key != KEY_TAB && key != KEY_1 && key != KEY_2) {
        keyboardFocus = true;
        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) return;
    }
    keyboardFocus = true;
    switch (key) {
        case KEY_LEFT:
            if (selected % 5 > 0) --selected;
            else if (p > 0) {
                setPage(p - 1, -1);
                selected = selected + 4;
            }
            break;
        case KEY_RIGHT:
            if (selected % 5 < 4 && selected + 1 < n) ++selected;
            else if (p < pages - 1) {
                setPage(p + 1, 1);
                selected = selected - selected % 5;
                int nn = std::min(10, (int)list.size() - (p + 1) * 10);
                if (selected >= nn) selected = 0;
            }
            break;
        case KEY_UP:
            if (selected >= 5) selected -= 5;
            break;
        case KEY_DOWN:
            if (selected + 5 < n) selected += 5;
            break;
        case KEY_ENTER:
        case KEY_KP_ENTER:
        case KEY_SPACE:
            if (p * 10 + selected < (int)list.size()) openGame(list[(size_t)(p * 10 + selected)], selected);
            break;
        case KEY_TAB: setTab((tab + 1) % tabCount()); break;
        case KEY_1: setTab(0); break;
        case KEY_2: setTab(1); break;
        case KEY_PAGE_UP:
        case KEY_Q: setPage(p - 1, -1); break;
        case KEY_PAGE_DOWN:
        case KEY_E: setPage(p + 1, 1); break;
        default: break;
    }
    n = std::min(10, (int)list.size() - page[tab] * 10);
    if (selected >= n) selected = std::max(0, n - 1);
}

static Rect tileRect(int slotIdx) {
    int col = slotIdx % 5, row = slotIdx / 5;
    return R(40.0f + col * 244.0f, 92.0f + row * 246.0f, 224.0f, 228.0f);
}

void SlotMachineUI::Impl::drawTile(int gi, int slotIdx, Rect r, float alpha) {
    const GameDef& g = catalog().games[(size_t)gi];
    const Theme& th = g.theme;
    float hv = (slotIdx >= 0 && slotIdx < 64) ? tileHover[slotIdx] : 0.0f;
    bool focus = keyboardFocus && slotIdx == selected && screen == 0;
    if (focus) hv = std::max(hv, 1.0f);
    float s = 1.0f + 0.045f * hv;
    G.pushAlpha(alpha);
    G.pushTransformAround(r.cx(), r.cy(), s);
    Color accent = hexc(th.accent);
    Color frame = hexc(th.frame);
    G.softRect(R(r.x, r.y + 8, r.w, r.h), 18, 14, hexA(0x000000, 0.65f));
    if (hv > 0.01f) G.softRect(inset(r, -5), 22, 18, withA(accent, 0.7f * hv));
    G.rectGrad(r, hexc(th.tileTop), hexc(th.tileBottom), 16);
    float ph = time * 0.22f + gi * 0.7f;
    G.rays(R(r.x, r.y - 10, r.w, r.h - 20), withA(lerpc(accent, hexc(0xffffff), 0.4f), 0.20f + 0.12f * hv),
           hexA(0xffffff, 0.0f), 12, ph, 16);
    G.radial(centered(r.cx(), r.y + 92, 210, 190), hexA(0xffffff, 0.28f + 0.1f * hv), hexA(0xffffff, 0.0f));
    // decor in functie de stilul temei
    if (th.style == 1) {
        for (int k = 0; k < 3; ++k) {
            float fx = r.x + 8 + k * 74, fh = 54 + 8 * std::sin(time * 6 + k * 2 + gi);
            G.pushAlpha(0.75f);
            G.sprite(SPR_FLAME, R(fx, r.y + 172 - fh, 60, fh + 6));
            G.popAlpha();
        }
    } else if (th.style == 2 || th.style == 4) {
        for (int k = 0; k < 4; ++k) {
            float tw = 0.5f + 0.5f * std::sin(time * 2.6f + k * 1.9f + gi);
            G.pushAlpha(tw * 0.9f);
            float sx = r.x + 20 + std::fmod(k * 61.0f + gi * 17.0f, r.w - 40);
            float sy = r.y + 18 + std::fmod(k * 37.0f + gi * 23.0f, 120.0f);
            G.sprite(SPR_SPARKLE, centered(sx, sy, 26, 26));
            G.popAlpha();
        }
    } else if (th.style == 3) {
        for (int k = 0; k < 5; ++k) {
            float t = std::fmod(time * 0.35f + k * 0.21f + gi * 0.13f, 1.0f);
            float bx = r.x + 22 + std::fmod(k * 47.0f + gi * 11.0f, r.w - 44);
            float by = r.y + 170 - t * 150;
            G.ring(bx, by, 5 + k % 3 * 2.0f, 1.6f, hexA(0xffffff, 0.45f * (1 - t)));
        }
    }
    float bob = std::sin(time * 1.8f + gi) * 3.0f;
    float ss = 148 + 10 * hv;
    G.sprite(g.thumbSprite, centered(r.cx(), r.y + 96 + bob, ss, ss));
    // banda cu titlul
    G.rectGrad(R(r.x, r.y + 150, r.w, r.h - 150), hexA(0x000000, 0.0f), hexA(0x000000, 0.82f), 16);
    TextStyle ts(th.titleFont, th.titleFont == FONT_SERIF ? 27.0f : 25.0f, hexc(th.titleTop));
    ts.grad(hexc(th.titleTop), hexc(th.titleBottom)).stroke(2.6f, hexc(th.titleOutline)).drop(2.5f).al(ALIGN_CENTER);
    G.textBox(R(r.x + 10, r.y + 176, r.w - 20, 40), g.name.c_str(), ts, 12);
    // etichete
    char lines[32];
    std::snprintf(lines, sizeof(lines), "%d LINII", g.lines);
    Rect lb = R(r.x + 10, r.y + 10, G.textWidth(lines, FONT_UI, 12) + 18, 22);
    G.rect(lb, hexA(0x000000, 0.55f), 11);
    TextStyle lt(FONT_UI, 12, hexc(0xffffff));
    lt.al(ALIGN_CENTER);
    G.textBox(lb, lines, lt);
    if (g.fsType != FS_NONE) {
        const char* sp = "SPECIALE";
        Rect sb = R(r.right() - 10 - (G.textWidth(sp, FONT_UI, 12) + 18), r.y + 10, G.textWidth(sp, FONT_UI, 12) + 18, 22);
        G.rectGrad(sb, hexc(0xffe68a), hexc(0xd08a12), 11);
        TextStyle st(FONT_UI, 12, hexc(0x2a1400));
        st.al(ALIGN_CENTER);
        G.textBox(sb, sp, st);
    }
    G.frame(r, 16, 2.0f + 1.5f * hv, withA(lerpc(frame, hexc(0xffffff), 0.35f * hv), 0.8f + 0.2f * hv));
    if (hv > 0.3f) {
        Rect pb = centered(r.cx(), r.y + 150, 104, 34);
        G.pushAlpha((hv - 0.3f) / 0.7f);
        G.softRect(pb, 17, 10, hexA(0x000000, 0.5f));
        G.rectBorder(pb, hexc(0x5ae87a), hexc(0x0f7a28), 17, 1.5f, hexc(0xc0ffd0));
        TextStyle pt(FONT_UI, 16, hexc(0xffffff));
        pt.al(ALIGN_CENTER).drop(1.2f);
        G.textBox(pb, SLOT_U8("JOACĂ"), pt);
        G.popAlpha();
    }
    G.popTransform();
    G.popAlpha();
}

void SlotMachineUI::Impl::drawPager(float y) {
    int pages = pageCount(tab), p = page[tab];
    const float bw = 50, gap = 12, aw = 64;
    float total = aw * 2 + pages * bw + (pages + 1) * gap;
    float x = 640 - total * 0.5f;
    Rect l = R(x, y - 25, aw, 50);
    button(l, "", ACT_PAGE_PREV, 0, BTN_GLASS, p > 0);
    G.triangle(l.cx() - 2, l.cy(), 20, 0, p > 0 ? hexc(0xffffff) : hexA(0xffffff, 0.3f));
    x += aw + gap;
    for (int i = 0; i < pages; ++i) {
        char num[16];
        std::snprintf(num, sizeof(num), "%d", i + 1);
        button(R(x, y - 25, bw, 50), num, ACT_PAGE, i, i == p ? BTN_GOLD : BTN_GLASS, true, i == p, 21);
        x += bw + gap;
    }
    Rect rr = R(x, y - 25, aw, 50);
    button(rr, "", ACT_PAGE_NEXT, 0, BTN_GLASS, p < pages - 1);
    G.triangle(rr.cx() + 2, rr.cy(), 20, 1, p < pages - 1 ? hexc(0xffffff) : hexA(0xffffff, 0.3f));
}

void SlotMachineUI::Impl::drawLobby() {
    const Rect full = R(0, 0, CANVAS_W, CANVAS_H);
    G.rectGrad(full, hexc(0x1c0d3e), hexc(0x060310));
    G.rays(centered(640, 330, 1800, 1800), hexA(0xffffff, 0.05f), hexA(0xffffff, 0.0f), 20, time * 0.04f);
    G.pattern(full, hexA(0x000000, 0.0f), hexA(0xffffff, 0.025f), 46);
    G.radial(centered(640, 360, 1700, 1150), hexA(0x000000, 0.0f), hexA(0x000000, 0.6f));

    // efect de apropiere spre pictograma aleasa
    bool zoom = transT < 0.5f && transTo == 1 && transTile >= 0;
    if (zoom) {
        Rect tr = tileRect(transTile);
        G.pushTransformAround(tr.cx(), tr.cy(), 1.0f + 0.6f * easeInOut(transT * 2.0f));
    }

    std::vector<int> list = gamesInTab(tab);
    int p = page[tab];
    hoverSlot = -1;
    // pictogramele jocurilor (2 randuri x 5 coloane)
    float e = easeOutCubic(pageAnimT);
    for (int pass = 0; pass < 2; ++pass) {
        int pg = p;
        float off = 0, alpha = easeOutCubic(tabAnimT);
        if (pageAnimT < 1.0f) {
            if (pass == 0) {
                pg = pageAnimFrom;
                off = -pageAnimDir * e * 1320.0f;
                alpha = 1.0f - e * 0.4f;
            } else {
                off = pageAnimDir * (1.0f - e) * 1320.0f;
            }
        } else if (pass == 0) {
            continue;
        }
        bool interactive = pass == 1 && pageAnimT >= 1.0f;
        for (int s = 0; s < 10; ++s) {
            int idx = pg * 10 + s;
            if (idx >= (int)list.size()) break;
            Rect r = tileRect(s);
            r.x += off;
            if (interactive) {
                addRegion(r, ACT_OPEN_GAME, list[(size_t)idx]);
                if (hovered(r)) hoverSlot = s;
            }
            drawTile(list[(size_t)idx], interactive ? s : -1, r, alpha);
        }
    }
    if (zoom) G.popTransform();

    // antet: tab-uri mici, jackpot, credit
    G.softRect(R(0, 70, CANVAS_W, 8), 0, 6, hexA(0x000000, 0.5f));
    G.rectGrad(R(0, 0, CANVAS_W, 76), hexc(0x2e1a5c), hexc(0x120828));
    G.rectGrad(R(0, 74, CANVAS_W, 3), hexc(0xffe07a), hexc(0x8a5a10));
    const Catalog& cat = catalog();
    for (int t = 0; t < (int)cat.tabNames.size(); ++t) {
        Rect tr = R(24.0f + t * 160.0f, 18, 148, 40);
        button(tr, cat.tabNames[(size_t)t].c_str(), ACT_TAB, t, t == tab ? BTN_GOLD : BTN_GLASS, true, t == tab, 16);
    }
    for (int j = 0; j < 4; ++j) drawJackpotBadge(R(366.0f + j * 158.0f, 12, 150, 52), j, 0);
    drawCreditBox(R(1004, 12, 252, 52), "CREDIT", creditShown, false);

    drawPager(620);

    // subsol
    G.rectGrad(R(0, 664, CANVAS_W, 56), hexA(0x000000, 0.0f), hexA(0x000000, 0.55f));
    char info[64];
    std::snprintf(info, sizeof(info), "PAGINA %d / %d", p + 1, pageCount(tab));
    TextStyle ft(FONT_UI, 15, hexA(0xd8ccff, 0.85f));
    ft.sp(1.0f);
    G.textBox(R(30, 684, 300, 24), info, ft);
    TextStyle ht(FONT_UI, 15, hexA(0xd8ccff, 0.85f));
    ht.al(ALIGN_CENTER);
    G.textBox(R(340, 684, 600, 24), SLOT_U8("Alege un joc  •  săgețile schimbă pagina"), ht);
    TextStyle ct(FONT_DISPLAY, 22, hexc(0xffffff));
    ct.al(ALIGN_RIGHT).drop(1.5f);
    G.textBox(R(1050, 682, 200, 28), clockString().c_str(), ct);
}

} // namespace slot
