// Catalogul jocurilor. Toate jocurile, numele si grafica sunt originale.
// Fisier UTF-8 (cu BOM). Sirurile cu diacritice se scriu cu SLOT_U8("...").
#include "GameCatalog.hpp"

#include "SymbolAtlas.hpp"

namespace slot {

const double BET_STEPS[] = {0.20, 0.40, 0.50, 1.00, 2.00, 3.00, 5.00, 10.00, 20.00, 50.00, 100.00, 500.00};
const int BET_STEP_COUNT = (int)(sizeof(BET_STEPS) / sizeof(BET_STEPS[0]));

int minBetStep(const GameDef& g) {
    for (int i = 0; i < BET_STEP_COUNT; ++i)
        if (BET_STEPS[i] / g.lines >= 0.00999) return i;
    return BET_STEP_COUNT - 1;
}

namespace {

const int F_DISPLAY = 1;   // FONT_DISPLAY din Renderer2D.hpp
const int F_SERIF = 2;     // FONT_SERIF

enum Style { ST_RAYS = 0, ST_FLAMES = 1, ST_STARS = 2, ST_BUBBLES = 3, ST_ICE = 4 };

Theme T(uint32_t bgTop, uint32_t bgBottom, uint32_t reelTop, uint32_t reelBottom, uint32_t frame, uint32_t accent,
        uint32_t titleTop, uint32_t titleBottom, uint32_t titleOutline, int font, int style,
        uint32_t tileTop, uint32_t tileBottom) {
    Theme t;
    t.bgTop = bgTop; t.bgBottom = bgBottom;
    t.reelTop = reelTop; t.reelBottom = reelBottom;
    t.frame = frame; t.accent = accent;
    t.titleTop = titleTop; t.titleBottom = titleBottom; t.titleOutline = titleOutline;
    t.titleFont = font; t.style = style;
    t.tileTop = tileTop; t.tileBottom = tileBottom;
    return t;
}

SymbolDef S(int sprite, int kind, float p2, float p3, float p4, float p5, uint32_t tint = 0) {
    SymbolDef d;
    d.sprite = sprite;
    d.kind = kind;
    d.pay[0] = 0; d.pay[1] = 0; d.pay[2] = p2; d.pay[3] = p3; d.pay[4] = p4; d.pay[5] = p5;
    d.tint = tint;
    return d;
}

// ------------------------------------------------------------------ jocuri clasice
struct ClassicSpec {
    int reels, rows, lines;
    int wild;          // 0 = fara wild, 1 = wild pe rolele 2-4, 2 = wild pe toate rolele
    bool expanding;    // wild-ul se extinde pe toata rola
    int scatter;       // sprite-ul scatter-ului sau -1
    int extra;         // un fruct in plus (mar / capsuna) sau -1
    int high;          // simbol mare (clopot, inima, potcoava, BAR...)
    int top;           // simbolul cel mai valoros (7, fulger, diamant)
};

GameDef classic(const char* id, const char* name, const ClassicSpec& sp, const Theme& th, int thumb, float scale) {
    GameDef g;
    g.id = id;
    g.name = name;
    g.tab = 0;
    g.reels = sp.reels;
    g.rows = sp.rows;
    g.lines = sp.lines;
    g.expandingWild = sp.expanding;
    g.fsType = FS_NONE;
    g.fsCount = 0;
    g.fsMultiplier = 1;
    g.scatterMin = 3;
    g.buyCost = 0;
    g.payScale = scale;
    g.thumbSprite = thumb;
    g.theme = th;
    std::vector<int> w;   // ponderea pe fiecare rola
    if (sp.reels == 3) {
        g.symbols.push_back(S(SPR_CHERRY, KIND_NORMAL, 1, 10, 0, 0));       w.push_back(5);
        g.symbols.push_back(S(SPR_LEMON, KIND_NORMAL, 0, 10, 0, 0));        w.push_back(6);
        g.symbols.push_back(S(SPR_ORANGE, KIND_NORMAL, 0, 10, 0, 0));       w.push_back(6);
        g.symbols.push_back(S(SPR_PLUM, KIND_NORMAL, 0, 10, 0, 0));         w.push_back(6);
        if (sp.extra >= 0) { g.symbols.push_back(S(sp.extra, KIND_NORMAL, 0, 15, 0, 0)); w.push_back(5); }
        g.symbols.push_back(S(SPR_GRAPES, KIND_NORMAL, 0, 25, 0, 0));       w.push_back(4);
        g.symbols.push_back(S(SPR_WATERMELON, KIND_NORMAL, 0, 25, 0, 0));   w.push_back(4);
        g.symbols.push_back(S(sp.high, KIND_NORMAL, 0, 40, 0, 0));          w.push_back(3);
        g.symbols.push_back(S(sp.top, KIND_NORMAL, 2, 100, 0, 0));          w.push_back(2);
        if (sp.scatter >= 0) { g.symbols.push_back(S(sp.scatter, KIND_SCATTER, 0, 5, 0, 0)); w.push_back(1); }
    } else {
        g.symbols.push_back(S(SPR_CHERRY, KIND_NORMAL, 1, 4, 10, 40));      w.push_back(7);
        g.symbols.push_back(S(SPR_LEMON, KIND_NORMAL, 0, 4, 10, 40));       w.push_back(8);
        g.symbols.push_back(S(SPR_ORANGE, KIND_NORMAL, 0, 4, 10, 40));      w.push_back(8);
        g.symbols.push_back(S(SPR_PLUM, KIND_NORMAL, 0, 4, 10, 40));        w.push_back(8);
        if (sp.extra >= 0) { g.symbols.push_back(S(sp.extra, KIND_NORMAL, 0, 6, 15, 60)); w.push_back(6); }
        g.symbols.push_back(S(SPR_GRAPES, KIND_NORMAL, 0, 10, 40, 120));    w.push_back(5);
        g.symbols.push_back(S(SPR_WATERMELON, KIND_NORMAL, 0, 10, 40, 120)); w.push_back(5);
        g.symbols.push_back(S(sp.high, KIND_NORMAL, 0, 20, 80, 300));       w.push_back(3);
        g.symbols.push_back(S(sp.top, KIND_NORMAL, 2, 50, 250, 1000));      w.push_back(2);
        if (sp.wild == 2) { g.symbols.push_back(S(SPR_WILD, KIND_WILD, 0, 25, 100, 500)); w.push_back(1); }
        else if (sp.wild == 1) { g.symbols.push_back(S(SPR_WILD, KIND_WILD, 0, 0, 0, 0)); w.push_back(2); }
        if (sp.scatter >= 0) { g.symbols.push_back(S(sp.scatter, KIND_SCATTER, 0, 2, 10, 50)); w.push_back(1); }
    }
    for (int c = 0; c < sp.reels; ++c) {
        std::vector<int> rw = w;
        if (sp.wild == 1 && (c == 0 || c == sp.reels - 1)) {
            for (size_t s = 0; s < g.symbols.size(); ++s)
                if (g.symbols[s].kind == KIND_WILD) rw[s] = 0;
        }
        g.weights.push_back(rw);
    }
    return g;
}

ClassicSpec C(int reels, int rows, int lines, int wild, bool expanding, int scatter, int extra, int high, int top) {
    ClassicSpec s = {reels, rows, lines, wild, expanding, scatter, extra, high, top};
    return s;
}

// ------------------------------------------------------------------ jocuri cu speciale
const uint32_t TINT_10 = 0xff9a2a, TINT_J = 0x3ad06a, TINT_Q = 0xb04aff, TINT_K = 0x3a8aff, TINT_A = 0xff3a4a;

// Tip "carte": simbolul special (wild + scatter) aduce rotiri gratuite in care un simbol ales se extinde.
GameDef bookGame(const char* id, const char* name, int rows, int lines, int p1, int p2, int p3, int p4, int special,
                 const Theme& th, int thumb, float scale) {
    GameDef g;
    g.id = id;
    g.name = name;
    g.tab = 1;
    g.reels = 5;
    g.rows = rows;
    g.lines = lines;
    g.expandingWild = false;
    g.fsType = FS_EXPANDING;
    g.fsCount = 10;
    g.fsMultiplier = 1;
    g.scatterMin = 3;
    g.buyCost = 100;
    g.payScale = scale;
    g.thumbSprite = thumb;
    g.theme = th;
    g.symbols.push_back(S(SPR_R10, KIND_NORMAL, 0, 5, 25, 100, TINT_10));
    g.symbols.push_back(S(SPR_RJ, KIND_NORMAL, 0, 5, 25, 100, TINT_J));
    g.symbols.push_back(S(SPR_RQ, KIND_NORMAL, 0, 5, 25, 100, TINT_Q));
    g.symbols.push_back(S(SPR_RK, KIND_NORMAL, 0, 5, 40, 150, TINT_K));
    g.symbols.push_back(S(SPR_RA, KIND_NORMAL, 0, 5, 40, 150, TINT_A));
    g.symbols.push_back(S(p1, KIND_NORMAL, 5, 30, 100, 750));
    g.symbols.push_back(S(p2, KIND_NORMAL, 5, 30, 100, 750));
    g.symbols.push_back(S(p3, KIND_NORMAL, 5, 40, 400, 2000));
    g.symbols.push_back(S(p4, KIND_NORMAL, 10, 100, 1000, 5000));
    g.symbols.push_back(S(special, KIND_BOOK, 0, 2, 20, 200));
    const int w[10] = {7, 7, 6, 6, 5, 4, 3, 2, 1, 1};
    for (int c = 0; c < 5; ++c) g.weights.push_back(std::vector<int>(w, w + 10));
    if (rows == 3) g.weights[2][9] = 2;   // rola din mijloc are doua simboluri speciale -> speciala ~1 la 200 de rotiri
    return g;
}

// Tip "multiplicator": wild separat, scatter separat, rotirile gratuite au castiguri multiplicate.
GameDef multGame(const char* id, const char* name, int p1, int p2, int p3, int p4, int scatter, int fsCount, int mult,
                 const Theme& th, int thumb, float scale) {
    GameDef g;
    g.id = id;
    g.name = name;
    g.tab = 1;
    g.reels = 5;
    g.rows = 3;
    g.lines = 20;
    g.expandingWild = false;
    g.fsType = FS_MULTIPLIER;
    g.fsCount = fsCount;
    g.fsMultiplier = mult;
    g.scatterMin = 3;
    g.buyCost = 100;
    g.payScale = scale;
    g.thumbSprite = thumb;
    g.theme = th;
    g.symbols.push_back(S(SPR_R10, KIND_NORMAL, 0, 5, 20, 100, TINT_10));
    g.symbols.push_back(S(SPR_RJ, KIND_NORMAL, 0, 5, 20, 100, TINT_J));
    g.symbols.push_back(S(SPR_RQ, KIND_NORMAL, 0, 5, 25, 100, TINT_Q));
    g.symbols.push_back(S(SPR_RK, KIND_NORMAL, 0, 10, 30, 125, TINT_K));
    g.symbols.push_back(S(SPR_RA, KIND_NORMAL, 0, 10, 30, 125, TINT_A));
    g.symbols.push_back(S(p1, KIND_NORMAL, 2, 15, 75, 250));
    g.symbols.push_back(S(p2, KIND_NORMAL, 2, 15, 75, 250));
    g.symbols.push_back(S(p3, KIND_NORMAL, 2, 25, 125, 750));
    g.symbols.push_back(S(p4, KIND_NORMAL, 5, 50, 250, 1500));
    g.symbols.push_back(S(SPR_WILD, KIND_WILD, 10, 100, 500, 2500));
    g.symbols.push_back(S(scatter, KIND_SCATTER, 2, 5, 20, 200));
    const int w[11] = {6, 6, 5, 5, 5, 4, 3, 3, 2, 1, 1};
    for (int c = 0; c < 5; ++c) g.weights.push_back(std::vector<int>(w, w + 11));
    g.weights[2][10] = 2;  // rola din mijloc are doua scatter-e
    return g;
}

// Joc de pescuit: pestii au valori; in rotirile gratuite pescarul (din fotografie) le strange.
GameDef fishingGame(const char* id, const char* name, const Theme& th, float scale) {
    GameDef g;
    g.id = id;
    g.name = name;
    g.tab = 1;
    g.reels = 5;
    g.rows = 3;
    g.lines = 10;
    g.expandingWild = false;
    g.fsType = FS_COLLECT;
    g.fsCount = 10;
    g.fsMultiplier = 1;
    g.scatterMin = 3;
    g.buyCost = 100;
    g.payScale = scale;
    g.thumbSprite = SPR_FISHER;
    g.theme = th;
    g.symbols.push_back(S(SPR_R10, KIND_NORMAL, 0, 5, 25, 100, TINT_10));
    g.symbols.push_back(S(SPR_RJ, KIND_NORMAL, 0, 5, 25, 100, TINT_J));
    g.symbols.push_back(S(SPR_RQ, KIND_NORMAL, 0, 5, 25, 100, TINT_Q));
    g.symbols.push_back(S(SPR_RK, KIND_NORMAL, 0, 5, 40, 150, TINT_K));
    g.symbols.push_back(S(SPR_RA, KIND_NORMAL, 0, 5, 40, 150, TINT_A));
    g.symbols.push_back(S(SPR_ROD, KIND_NORMAL, 0, 20, 100, 400));
    g.symbols.push_back(S(SPR_BOAT, KIND_NORMAL, 0, 25, 150, 600));
    g.symbols.push_back(S(SPR_ANCHOR, KIND_NORMAL, 5, 40, 250, 1000));
    g.symbols.push_back(S(SPR_FISH_WILD, KIND_WILD, 10, 100, 500, 2000));
    g.symbols.push_back(S(SPR_FISH_MONEY, KIND_MONEY, 0, 0, 0, 0));
    g.symbols.push_back(S(SPR_BUOY, KIND_SCATTER, 0, 2, 20, 200));
    g.symbols.push_back(S(SPR_FISHER, KIND_COLLECTOR, 0, 0, 0, 0));
    const int w[12] = {7, 7, 6, 6, 5, 4, 3, 2, 1, 4, 1, 0};     // jocul de baza (fara pescar)
    const int f[12] = {6, 6, 5, 5, 5, 3, 3, 2, 1, 5, 1, 2};     // rotirile gratuite (cu pescar)
    for (int c = 0; c < 5; ++c) {
        g.weights.push_back(std::vector<int>(w, w + 12));
        g.fsWeights.push_back(std::vector<int>(f, f + 12));
    }
    g.weights[2][10] = 2;
    return g;
}

Catalog buildCatalog() {
    Catalog cat;
    // Numele tab-urilor (primul tab este selectat implicit)
    cat.tabNames.push_back("CLASICE");
    cat.tabNames.push_back("SPECIALE");

    // ---------------------------------------------------------------- teme
    const Theme FIRE = T(0x3a0500, 0x100000, 0x240600, 0x0a0100, 0xffb020, 0xff5a00, 0xfff27a, 0xff6a00, 0x3a0800, F_DISPLAY, ST_FLAMES, 0xff5a00, 0x4a0000);
    const Theme PURPLE = T(0x2a0a4a, 0x0a0218, 0x1a0838, 0x070212, 0xffd040, 0xc040ff, 0xffffff, 0xffc040, 0x2a0050, F_DISPLAY, ST_RAYS, 0x8a2be2, 0x2a0a5a);
    const Theme GOLD = T(0x3a2a00, 0x100800, 0xfff6dc, 0xe2cc9a, 0xffd040, 0xffb000, 0xfff6c0, 0xd08a10, 0x3a1a00, F_DISPLAY, ST_RAYS, 0xffc83a, 0x6a3a00);
    const Theme NIGHT = T(0x0a1440, 0x02040f, 0x101a48, 0x040818, 0x9ad0ff, 0x4080ff, 0xffffff, 0x9ad0ff, 0x001040, F_DISPLAY, ST_STARS, 0x2040a0, 0x050a30);
    const Theme EMERALD = T(0x003a20, 0x000f08, 0x0a2a1a, 0x020a06, 0xffd040, 0x20d080, 0xfff6c0, 0xffb020, 0x002a10, F_DISPLAY, ST_RAYS, 0x10a060, 0x003a20);
    const Theme ICE = T(0x0a3a5a, 0x021020, 0xeefaff, 0xb4dcef, 0xc8f0ff, 0x40d0ff, 0xffffff, 0x80e0ff, 0x003a5a, F_DISPLAY, ST_ICE, 0x60c8f0, 0x0a3a6a);
    const Theme NEON = T(0x1a0030, 0x05000c, 0x0a0018, 0x020006, 0xff40d0, 0x40fff0, 0xffffff, 0xff60e0, 0x3a0050, F_DISPLAY, ST_STARS, 0xff2ad0, 0x2a0060);
    const Theme CLOVER = T(0x0a4a1a, 0x021206, 0x123a1a, 0x041008, 0xffe060, 0x40ff60, 0xe8ffd0, 0x40d040, 0x002a08, F_DISPLAY, ST_STARS, 0x30c040, 0x0a3a10);
    const Theme GARDEN = T(0x3a0a2a, 0x10020a, 0xfff0f6, 0xf0cfe0, 0x60d060, 0xff60a0, 0xffffff, 0xff80b0, 0x3a0020, F_DISPLAY, ST_BUBBLES, 0xff6ab0, 0x6a0a3a);
    const Theme LAVA = T(0x4a0a00, 0x100000, 0x200400, 0x080000, 0xff7a00, 0xff3000, 0xffe040, 0xff3000, 0x2a0000, F_DISPLAY, ST_FLAMES, 0xff4000, 0x3a0000);
    const Theme WESTERN = T(0x4a2a10, 0x140a04, 0xf6e6c8, 0xd8ba8e, 0xc08a40, 0xffb040, 0xfff0c0, 0xc07a20, 0x2a1404, F_SERIF, ST_RAYS, 0xc0803a, 0x4a2a10);
    const Theme RETRO = T(0x2a1a0a, 0x0a0602, 0xfff8e8, 0xecdab6, 0xff3a2a, 0xffd040, 0xffffff, 0xffd040, 0x5a0a00, F_DISPLAY, ST_RAYS, 0xff5a3a, 0x7a1a0a);
    const Theme RUBY = T(0x4a0018, 0x120006, 0x2a0010, 0x0a0004, 0xffc0d0, 0xff3060, 0xffffff, 0xff6080, 0x3a0010, F_DISPLAY, ST_STARS, 0xff3a6a, 0x5a0020);
    const Theme RAINBOW = T(0x2a0a5a, 0x0a0218, 0x1a0a3a, 0x05020f, 0xffd040, 0x40d0ff, 0xfff27a, 0xff40a0, 0x200040, F_DISPLAY, ST_RAYS, 0xffa020, 0x7a2be2);
    const Theme SUMMER = T(0x0a6ab0, 0x02203a, 0xfffbe8, 0xffe4a8, 0xffa020, 0xffd040, 0xffffff, 0xffb020, 0x0a3a6a, F_DISPLAY, ST_RAYS, 0x40c0ff, 0x0a4a8a);
    const Theme ELECTRIC = T(0x001a4a, 0x00050f, 0x001030, 0x000410, 0x40e0ff, 0xfff040, 0xffffff, 0x60e0ff, 0x002040, F_DISPLAY, ST_STARS, 0x2080ff, 0x001040);
    const Theme ROYAL = T(0x0a1a5a, 0x020618, 0x0a1440, 0x030614, 0xffd040, 0x4060ff, 0xfff6c0, 0xffc040, 0x0a0a3a, F_SERIF, ST_RAYS, 0x2a4ad0, 0x0a0a4a);
    const Theme WINE = T(0x3a0020, 0x0f0008, 0x2a0818, 0x0a0206, 0xd0a060, 0xa02060, 0xffe8f0, 0xd06090, 0x2a0010, F_SERIF, ST_RAYS, 0x8a1a4a, 0x2a0010);
    const Theme SILVER = T(0x2a3040, 0x0a0c12, 0xf4f6fa, 0xccd4e2, 0xd8e0ec, 0x80a0ff, 0xffffff, 0xa8b8d0, 0x1a2030, F_DISPLAY, ST_STARS, 0x8a98b0, 0x2a3040);
    const Theme HOTSUN = T(0x5a1a00, 0x1a0400, 0x2a0a00, 0x0a0200, 0xffc040, 0xff8000, 0xfff27a, 0xff6000, 0x3a0a00, F_DISPLAY, ST_FLAMES, 0xffa020, 0x8a1a00);
    const Theme MONEY = T(0x0a3a1a, 0x020f06, 0x0a2a12, 0x020a04, 0xffd040, 0x40ff80, 0xe8ffe0, 0x40e070, 0x002a10, F_DISPLAY, ST_RAYS, 0x20a050, 0x053a18);
    const Theme POLAR = T(0x0a2a5a, 0x020a1a, 0x0a1a3a, 0x020814, 0xe0f0ff, 0x80d0ff, 0xffffff, 0xa0e0ff, 0x002050, F_DISPLAY, ST_ICE, 0x4a8ad0, 0x0a1a4a);
    const Theme PINK = T(0x4a0a3a, 0x120210, 0x2a0820, 0x0a0208, 0xffd0f0, 0xff40c0, 0xffffff, 0xff80d0, 0x3a0030, F_DISPLAY, ST_BUBBLES, 0xff60c0, 0x5a0a4a);
    const Theme ORCHARD = T(0x1a3a0a, 0x060f02, 0xf6fae8, 0xd6e4b8, 0xffc040, 0xff4030, 0xfff6c0, 0xffb020, 0x2a1a00, F_SERIF, ST_RAYS, 0x8ac040, 0x2a4a0a);
    const Theme CANDY = T(0x5a0a2a, 0x1a020a, 0xfff0f4, 0xffcfdb, 0xff80b0, 0xff3070, 0xffffff, 0xff4080, 0x4a0020, F_DISPLAY, ST_BUBBLES, 0xff4a7a, 0x7a0a2a);

    const Theme MAGIC = T(0x2a0a4a, 0x08020f, 0x1a0a30, 0x06020c, 0xd0a0ff, 0xa040ff, 0xffffff, 0xc080ff, 0x1a0030, F_SERIF, ST_STARS, 0x6a2ad0, 0x1a0440);
    const Theme EGYPT = T(0x5a3a10, 0x1a0e04, 0x2a1a08, 0x0a0602, 0xffd040, 0x40c0ff, 0xfff6c0, 0xffb020, 0x2a1400, F_SERIF, ST_RAYS, 0xd09030, 0x4a2a08);
    const Theme PIRATE = T(0x0a3a5a, 0x020f1a, 0x2a1a0a, 0x0a0602, 0xc89040, 0xffd040, 0xfff0c0, 0xd09040, 0x1a0a00, F_SERIF, ST_BUBBLES, 0x1a6a9a, 0x0a2a3a);
    const Theme ASIAN = T(0x5a0a0a, 0x1a0202, 0x2a0606, 0x0a0202, 0xffc040, 0xff4020, 0xfff27a, 0xffb020, 0x3a0000, F_SERIF, ST_RAYS, 0xd02a1a, 0x4a0606);
    const Theme VIKING = T(0x1a2a3a, 0x050a10, 0x10202a, 0x04080c, 0xb8c8d8, 0x60c0ff, 0xffffff, 0xa0c0e0, 0x0a1420, F_SERIF, ST_ICE, 0x4a6a8a, 0x10202a);
    const Theme FOREST = T(0x0a3a2a, 0x02100a, 0x0a2a1a, 0x020a06, 0xc8e080, 0x60ff90, 0xf0ffd0, 0x80e060, 0x0a2a0a, F_SERIF, ST_STARS, 0x2a8a5a, 0x0a2a1a);
    const Theme GREEK = T(0x0a2a6a, 0x020a20, 0x0a1a40, 0x020614, 0xffe080, 0xffffff, 0xffffff, 0xffd060, 0x0a1a4a, F_SERIF, ST_RAYS, 0x3a6ad0, 0x0a1a4a);
    const Theme ARABIAN = T(0x3a0a4a, 0x0f0214, 0x2a0838, 0x0a020f, 0xffc040, 0xff60d0, 0xfff6c0, 0xffb020, 0x2a0030, F_SERIF, ST_STARS, 0x9a2ab0, 0x2a0838);
    const Theme OCEAN = T(0x003a5a, 0x000f1a, 0x002a40, 0x000a14, 0x80e0ff, 0x40ffe0, 0xffffff, 0x80f0ff, 0x002a3a, F_SERIF, ST_BUBBLES, 0x0a8ab0, 0x002a40);
    const Theme AZTEC = T(0x1a3a0a, 0x060f02, 0x1a2a0a, 0x060a02, 0xffc040, 0x20d0c0, 0xfff6c0, 0xffb020, 0x1a1000, F_SERIF, ST_RAYS, 0x5a8a1a, 0x1a2a06);
    const Theme HAUNTED = T(0x1a0a2a, 0x05020a, 0x10081a, 0x040208, 0x90ff90, 0x80ff40, 0xe0ffe0, 0x80ff60, 0x0a1a0a, F_SERIF, ST_ICE, 0x3a2a5a, 0x0a0614);
    const Theme ALCHEMY = T(0x3a2a0a, 0x0f0a02, 0x2a1a08, 0x0a0602, 0xd8b060, 0xff8a20, 0xfff0c0, 0xd09030, 0x2a1400, F_SERIF, ST_STARS, 0x8a6a2a, 0x2a1a08);
    const Theme TREASURE = T(0x2a3a0a, 0x0a0f02, 0x2a1a0a, 0x0a0602, 0xffc040, 0xffd040, 0xfff6c0, 0xffb020, 0x2a1a00, F_SERIF, ST_RAYS, 0x6a8a2a, 0x2a1a0a);
    const Theme JADE = T(0x0a3a3a, 0x020f0f, 0x0a2a2a, 0x020a0a, 0xffd040, 0x40ffc0, 0xf0fff8, 0x60e0b0, 0x002a20, F_SERIF, ST_ICE, 0x1a9a7a, 0x0a2a2a);
    const Theme LAKE = T(0x0a4a6a, 0x021a2a, 0x0a3a5a, 0x021624, 0x9ae0ff, 0x40d0ff, 0xffffff, 0x9af0ff, 0x002a3a, F_DISPLAY, ST_BUBBLES, 0x1a8ac0, 0x053a5a);
    const Theme ASTRAL = T(0x0a0a3a, 0x02020f, 0x0a0a2a, 0x02020a, 0xd0d8ff, 0x8080ff, 0xffffff, 0xb0b8ff, 0x0a0a30, F_SERIF, ST_STARS, 0x4a4ad0, 0x0a0a3a);

    std::vector<GameDef>& G = cat.games;
    // ---------------------------------------------------------------- CLASICE
    //                         role rnd linii wild  extins scatter      fruct extra      simbol mare    simbol top
    G.push_back(classic("foc5", SLOT_U8("Fructe de Foc"), C(5, 3, 5, 0, false, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), FIRE, SPR_SEVEN, 5.1612f));
    G.push_back(classic("septari10", SLOT_U8("Șeptari Norocoși"), C(5, 3, 10, 1, true, SPR_COIN, -1, SPR_BELL, SPR_SEVEN), PURPLE, SPR_SEVEN, 1.2681f));
    G.push_back(classic("cirese3", SLOT_U8("Cireșe Aurii"), C(3, 3, 5, 0, false, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), GOLD, SPR_CHERRY, 3.3364f));
    G.push_back(classic("stele20", SLOT_U8("Ploaie de Stele"), C(5, 3, 20, 1, false, SPR_STAR, SPR_APPLE, SPR_BELL, SPR_SEVEN), NIGHT, SPR_STAR, 4.0882f));
    G.push_back(classic("clopote40", SLOT_U8("Clopote de Aur"), C(5, 4, 40, 2, false, SPR_COIN, -1, SPR_BELL, SPR_SEVEN), EMERALD, SPR_BELL, 3.2315f));
    G.push_back(classic("gheata10", SLOT_U8("Fructe de Gheață"), C(5, 3, 10, 0, false, SPR_DIAMOND, -1, SPR_BELL, SPR_SEVEN), ICE, SPR_LEMON, 5.1608f));
    G.push_back(classic("neon5", SLOT_U8("Neon 777"), C(5, 3, 5, 0, false, SPR_STAR, -1, SPR_BAR, SPR_SEVEN), NEON, SPR_SEVEN, 5.1781f));
    G.push_back(classic("trifoi20", SLOT_U8("Trifoi Magic"), C(5, 3, 20, 1, false, SPR_CLOVER, -1, SPR_HORSESHOE, SPR_SEVEN), CLOVER, SPR_CLOVER, 3.0487f));
    G.push_back(classic("gradina40", SLOT_U8("Grădina cu Fructe"), C(5, 4, 40, 1, false, SPR_STAR, SPR_STRAWBERRY, SPR_BELL, SPR_SEVEN), GARDEN, SPR_STRAWBERRY, 3.9314f));
    G.push_back(classic("lava40", SLOT_U8("Lava 40"), C(5, 4, 40, 1, true, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), LAVA, SPR_FLAME, 0.8747f));
    G.push_back(classic("potcoava10", SLOT_U8("Potcoava Norocoasă"), C(5, 3, 10, 1, false, SPR_COIN, -1, SPR_HORSESHOE, SPR_SEVEN), WESTERN, SPR_HORSESHOE, 3.0513f));
    G.push_back(classic("retro3", SLOT_U8("Fructe Retro"), C(3, 3, 5, 0, false, -1, -1, SPR_BAR, SPR_SEVEN), RETRO, SPR_BAR, 3.1053f));
    G.push_back(classic("rubin20", SLOT_U8("Inimă de Rubin"), C(5, 3, 20, 1, false, SPR_STAR, -1, SPR_HEART, SPR_SEVEN), RUBY, SPR_HEART, 3.0407f));
    G.push_back(classic("mega100", SLOT_U8("Mega Fructe 100"), C(5, 4, 100, 1, false, SPR_STAR, SPR_APPLE, SPR_BELL, SPR_SEVEN), RAINBOW, SPR_GRAPES, 3.9931f));
    G.push_back(classic("vara10", SLOT_U8("Soarele Verii"), C(5, 3, 10, 1, true, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), SUMMER, SPR_WATERMELON, 1.2878f));
    G.push_back(classic("fulger20", SLOT_U8("Fulger Electric"), C(5, 3, 20, 1, false, SPR_COIN, -1, SPR_BELL, SPR_LIGHTNING), ELECTRIC, SPR_LIGHTNING, 3.0546f));
    G.push_back(classic("diamante20", SLOT_U8("Diamante Regale"), C(5, 3, 20, 1, false, SPR_STAR, -1, SPR_HEART, SPR_DIAMOND), ROYAL, SPR_DIAMOND, 3.0433f));
    G.push_back(classic("struguri10", SLOT_U8("Struguri de Aur"), C(5, 3, 10, 0, false, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), WINE, SPR_GRAPES, 5.1547f));
    G.push_back(classic("clopotel3", SLOT_U8("Clopoțel de Argint"), C(3, 3, 5, 0, false, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), SILVER, SPR_BELL, 3.3158f));
    G.push_back(classic("vara40", SLOT_U8("Vară Fierbinte"), C(5, 4, 40, 1, true, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), HOTSUN, SPR_ORANGE, 0.8922f));
    G.push_back(classic("bani10", SLOT_U8("Bani Gheață"), C(5, 3, 10, 1, false, SPR_COIN, -1, SPR_BAR, SPR_SEVEN), MONEY, SPR_COIN, 3.0450f));
    G.push_back(classic("polara5", SLOT_U8("Stea Polară"), C(5, 3, 5, 0, false, SPR_STAR, -1, SPR_BELL, SPR_SEVEN), POLAR, SPR_STAR, 5.1786f));
    G.push_back(classic("cocktail100", SLOT_U8("Cocktail de Fructe"), C(5, 4, 100, 1, false, SPR_STAR, SPR_STRAWBERRY, SPR_HEART, SPR_SEVEN), PINK, SPR_CHERRY, 4.0030f));
    G.push_back(classic("mar10", SLOT_U8("Mărul de Aur"), C(5, 3, 10, 1, false, SPR_STAR, SPR_APPLE, SPR_BELL, SPR_SEVEN), ORCHARD, SPR_APPLE, 4.0936f));
    G.push_back(classic("capsuni3", SLOT_U8("Căpșuni Dulci"), C(3, 3, 5, 0, false, SPR_STAR, SPR_STRAWBERRY, SPR_HEART, SPR_SEVEN), CANDY, SPR_STRAWBERRY, 4.2452f));

    // ---------------------------------------------------------------- SPECIALE
    //                   rnd linii  P1 (mic)        P2              P3              P4 (mare)       special
    G.push_back(fishingGame("pescar", SLOT_U8("Pescarul Norocos"), LAKE, 0.92f));
    G.push_back(bookGame("grimoar", SLOT_U8("Grimoarul Magului"), 3, 10, SPR_POTION, SPR_HOURGLASS, SPR_MOON, SPR_ORB, SPR_BOOK_MAGIC, MAGIC, SPR_BOOK_MAGIC, 0.9488f));
    G.push_back(bookGame("nil", SLOT_U8("Nisipurile Nilului"), 3, 10, SPR_LOTUS, SPR_ANKH, SPR_EYE, SPR_PYRAMID, SPR_SCROLL, EGYPT, SPR_PYRAMID, 0.9548f));
    G.push_back(bookGame("pirati", SLOT_U8("Comoara Piraților"), 3, 10, SPR_ANCHOR, SPR_COMPASS, SPR_SWORD, SPR_SKULL, SPR_MAP, PIRATE, SPR_MAP, 0.9533f));
    G.push_back(bookGame("dragon", SLOT_U8("Cartea Dragonului"), 3, 10, SPR_LANTERN, SPR_COIN_ASIAN, SPR_JADE, SPR_RUBY, SPR_BOOK_DRAGON, ASIAN, SPR_LANTERN, 0.9406f));
    G.push_back(bookGame("vikingi", SLOT_U8("Regatul Vikingilor"), 3, 10, SPR_AXE, SPR_SHIELD, SPR_KEY, SPR_HELMET, SPR_RUNE, VIKING, SPR_HELMET, 0.9384f));
    G.push_back(bookGame("padure", SLOT_U8("Pădurea Fermecată"), 3, 10, SPR_FEATHER, SPR_MUSHROOM, SPR_MOON, SPR_EMERALD, SPR_BOOK, FOREST, SPR_MUSHROOM, 0.9521f));
    G.push_back(bookGame("olimp", SLOT_U8("Fulgerul Olimpului"), 3, 10, SPR_LYRE, SPR_AMPHORA, SPR_TRIDENT, SPR_LIGHTNING, SPR_SCROLL, GREEK, SPR_LIGHTNING, 0.9368f));
    G.push_back(bookGame("lampa", SLOT_U8("Lampa Fermecată"), 3, 10, SPR_RING, SPR_SWORD, SPR_CHALICE, SPR_RUBY, SPR_LAMP, ARABIAN, SPR_LAMP, 0.9549f));
    G.push_back(bookGame("abis", SLOT_U8("Perla Abisului"), 3, 10, SPR_STARFISH, SPR_SHELL, SPR_ANCHOR, SPR_TRIDENT, SPR_PEARL, OCEAN, SPR_PEARL, 0.9375f));
    G.push_back(bookGame("soare", SLOT_U8("Templul Soarelui"), 3, 10, SPR_FEATHER, SPR_EMERALD, SPR_SUN, SPR_MASK, SPR_SUNDISK, AZTEC, SPR_MASK, 0.9313f));
    G.push_back(multGame("castel", SLOT_U8("Castelul Bântuit"), SPR_LANTERN, SPR_POTION, SPR_AMETHYST, SPR_SKULL, SPR_MOON, 15, 3, HAUNTED, SPR_SKULL, 1.2758f));
    G.push_back(multGame("alchimist", SLOT_U8("Cronica Alchimistului"), SPR_KEY, SPR_RING, SPR_HOURGLASS, SPR_POTION, SPR_BOOK, 12, 2, ALCHEMY, SPR_POTION, 1.4112f));
    G.push_back(bookGame("comori", SLOT_U8("Vânătorul de Comori"), 4, 20, SPR_KEY, SPR_COMPASS, SPR_AMETHYST, SPR_CHALICE, SPR_CHEST, TREASURE, SPR_CHEST, 0.6090f));
    G.push_back(bookGame("jad", SLOT_U8("Muntele de Jad"), 4, 20, SPR_LOTUS, SPR_LANTERN, SPR_COIN_ASIAN, SPR_JADE, SPR_BOOK_DRAGON, JADE, SPR_JADE, 0.6094f));
    G.push_back(multGame("stea", SLOT_U8("Steaua Nordului"), SPR_MOON, SPR_EYE, SPR_SAPPHIRE, SPR_SUN, SPR_STAR, 15, 3, ASTRAL, SPR_SUN, 1.2689f));
    return cat;
}

} // namespace

const Catalog& catalog() {
    static const Catalog c = buildCatalog();
    return c;
}

std::vector<int> gamesInTab(int tab) {
    std::vector<int> out;
    const Catalog& c = catalog();
    for (size_t i = 0; i < c.games.size(); ++i)
        if (c.games[i].tab == tab) out.push_back((int)i);
    return out;
}

} // namespace slot
