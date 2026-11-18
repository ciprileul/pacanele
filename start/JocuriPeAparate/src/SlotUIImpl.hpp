#pragma once
// Detalii interne ale SlotMachineUI (nu trebuie incluse in proiectul tau).
#include "GameCatalog.hpp"
#include "Renderer2D.hpp"
#include "SlotAudio.hpp"
#include "SlotEngine.hpp"
#include "SlotUI.hpp"
#include "SymbolAtlas.hpp"

#include <string>
#include <vector>

namespace slot {

enum Action {
    ACT_NONE = 0,
    ACT_BLOCK,          // zona care blocheaza clicurile sub o fereastra
    ACT_TAB,            // param = tab
    ACT_PAGE,           // param = pagina
    ACT_PAGE_PREV,
    ACT_PAGE_NEXT,
    ACT_OPEN_GAME,      // param = index in catalog
    ACT_START,
    ACT_BET_MINUS,
    ACT_BET_PLUS,
    ACT_AUTO,
    ACT_GAMBLE,
    ACT_INFO,
    ACT_MENU,
    ACT_BUY,
    ACT_GAMBLE_RED,
    ACT_GAMBLE_BLACK,
    ACT_GAMBLE_COLLECT,
    ACT_INFO_PREV,
    ACT_INFO_NEXT,
    ACT_INFO_CLOSE,
    ACT_BUY_YES,
    ACT_BUY_NO,
    ACT_JP_PICK,        // param = cartea
    ACT_AUTO_PICK,      // param = indexul optiunii din panoul AUTO
    ACT_AUTO_GO,
    ACT_AUTO_SPEED,     // param = 0 normal, 1 rapid, 2 turbo
    ACT_AUTO_CANCEL,
    ACT_CONTINUE        // continua dupa o fereastra (speciala, castig mare)
};

enum BtnStyle { BTN_DARK = 0, BTN_GOLD, BTN_GREEN, BTN_RED, BTN_BLACK, BTN_GLASS };

enum Overlay { OV_NONE = 0, OV_FS_INTRO, OV_FS_OUTRO, OV_GAMBLE, OV_INFO, OV_BUY, OV_JACKPOT, OV_AUTO };

// optiunile din panoul AUTO (numar de rotiri; -1 = pana la oprire)
static const int AUTO_OPTIONS[] = {10, 25, 50, 100, 250, 500, -1};
static const int AUTO_OPTION_COUNT = 7;

struct Region {
    Rect r;
    int action;
    int param;
};

struct InputEvent {
    int type;     // 0 = click, 1 = tasta, 2 = scroll
    float x, y;   // coordonate virtuale (1280x720)
    int key;
};

struct ReelAnim {
    float pos;          // pozitia benzii (in simboluri); randul de sus arata strip[floor(pos)]
    float speed;        // simboluri / secunda
    int phase;          // 0 oprit, 1 pornire, 2 rotire, 3 franare, 4 revenire
    float t;            // timp in faza curenta
    float stopAt;       // momentul (de la pornire) cand incepe franarea
    float from, to;     // pentru franare / revenire
    float dur;
    int target;         // pozitia finala in banda
    bool anticipation;  // asteptare dupa scatter (rola se invarte mai mult)
    bool fast;          // oprire rapida ("maini rapide" / AUTO turbo): franare si revenire scurte
};

struct Particle {
    float x, y, vx, vy, rot, spin, life, size;
    int sprite;
};

struct SlotMachineUI::Impl {
    explicit Impl(int w, int h, Wallet* wallet);
    ~Impl();

    // ---------------------------------------------------------- comun
    int w, h;
    GLuint fbo, tex;
    GLuint topFbo, topTex;        // ecranul de sus (creat la primul renderTop)
    int mipLevels;
    float vscale, voffX, voffY;   // maparea textura -> panza virtuala
    Wallet ownWallet;
    Wallet* wallet;
    float time;
    float mouseX, mouseY;
    bool mouseInside, mouseHeld;
    bool keyboardFocus;
    bool drawingOverlay;        // butoanele raspund la mouse doar pe stratul de sus
    int hoverSlot;              // pictograma de sub mouse in lobby (-1 = niciuna)
    std::vector<Region> regions;
    std::vector<InputEvent> events;
    std::vector<Particle> particles;
    double creditShown;
    std::string toast;
    float toastT;
    Renderer2D& G;
    int audioOwner;
    int spinVoice, anticVoice, countVoice;
    void snd(int s, float vol = 1.0f) { audio::play(s, audioOwner, vol); }

    // ---------------------------------------------------------- lobby
    int screen;                 // 0 lobby, 1 joc
    int tab;
    int page[8];
    int selected;               // pozitia selectata cu tastatura (0..9)
    float pageAnimT;            // 0..1 (1 = terminat)
    int pageAnimDir, pageAnimFrom;
    float tabAnimT;
    float tileHover[64];
    float transT;               // tranzitie lobby <-> joc (>= 1 = terminata)
    int transTo;
    int transGame;
    int transTile;

    // ---------------------------------------------------------- joc
    int gameIndex;
    const GameDef* game;
    SlotEngine engine;
    int betStep;
    ReelAnim reels[MAX_REELS];
    bool spinning;
    float spinT;
    SpinResult result;
    bool haveResult;
    bool presenting;
    float presentT;
    double pendingWin;          // castig necolectat (se poate dubla)
    double winShown;            // contorul animat din caseta CASTIG
    double winTarget;
    double winFrom;
    float winCountT, winCountDur;
    float expandT;              // animatia simbolului special (>= 0 activ)
    float collectT;             // animatia pescarului cu cosul (>= 0 activ)
    bool fishCollected;         // pestii au intrat in cos (ascunsi pana la urmatoarea rotire)
    int collectN, collectMult;  // cati pesti zboara si de cate ori se plateste (cati pescari)
    int collectCells[MAX_REELS * MAX_ROWS][2];
    int expandReels;
    float bigWinT;              // >= 0 cand fereastra de castig mare e activa
    double bigWinAmount;
    bool autoPlay;
    int autoLeft;               // rotiri AUTO ramase (-1 = fara limita)
    int autoSel;                // optiunea selectata in panoul AUTO
    int autoSpeed;              // viteza AUTO: 0 normala, 1 rapida, 2 turbo
    int speedNow() const { return autoPlay ? autoSpeed : 0; }
    float idleT;
    int overlay;
    float overlayT;
    std::string message;
    std::string message2;

    // rotiri speciale
    bool fsActive;
    bool reelFs;                // rolele afiseaza benzile rotirilor gratuite
    int fsLeft, fsPlayed, fsTotal;
    double fsWon;
    int special;                // simbolul special (index in GameDef::symbols)
    int fsPendingAward;         // rotiri castigate care incep dupa prezentare
    float fsRetriggerT;
    bool buying;

    // dublaj
    double gambleAmount;
    int gambleSteps;
    int gambleHistory[6];
    int gambleHistCount;
    int gambleCard;             // ultima carte: culoare 0..3 (inima, romb, trefla, pica), -1 = acoperita
    int gambleRank;
    float gambleFlipT;          // >= 0 in timpul intoarcerii cartii
    int gambleResult;           // 0 nimic, 1 castigat, 2 pierdut
    float gambleResultT;

    // evidentierea castigurilor
    bool hl[MAX_REELS][MAX_ROWS];
    bool hlOn;
    int activeWin;              // -1 = toate castigurile, altfel indexul in result.wins

    // informatii
    int infoPage;

    // jackpot
    int jpTierPending;          // nivelul castigat la ultima rotire, inca neafisat
    int jpTier;
    double jpAmount;
    int jpCards[12];            // -1 = intoarsa cu fata in jos
    int jpSeq[12];
    int jpSeqLen, jpPicks;
    int jpSel;
    float jpDoneT;
    bool jpPaid;

    // ---------------------------------------------------------- metode
    void update(float dt);
    void render();
    void renderTop();
    void drawTopScreen();
    void handleEvent(const InputEvent& e);
    void handleKey(int key);
    void doAction(int action, int param);
    void toVirtual(float u, float v, float& x, float& y) const;
    bool busy() const;
    bool celebrating() const;

    // desen comun
    bool button(Rect r, const char* label, int action, int param, int style, bool enabled = true, bool active = false, float fontSize = 0);
    void addRegion(Rect r, int action, int param);
    bool hovered(Rect r) const;
    void drawJackpotBadge(Rect r, int tier, float pulse);
    void drawCreditBox(Rect r, const char* label, double value, bool highlight);
    void drawParticles();
    void spawnCoins(int n, float x, float y, float spread);
    void showToast(const std::string& s);
    void drawToast();

    // lobby
    int tabCount() const;
    int pageCount(int t) const;
    void setPage(int p, int dir);
    void setTab(int t);
    void openGame(int catalogIndex, int tileSlot);
    void drawLobby();
    void drawTile(int catalogIndex, int slot, Rect r, float alpha);
    void drawPager(float y);
    void lobbyKey(int key);

    // joc
    void enterGame(int catalogIndex);
    void leaveGame();
    double bet() const;
    double lineBet() const;
    void startSpin();
    void launchReels();
    void computeHighlights();
    void startBuy();
    void quickStop();
    void updateGame(float dt);
    void updateReels(float dt);
    void onReelsStopped();
    void beginPresentation();
    float presentationMinTime() const;
    void collect();
    void changeBet(int d);
    void startFreeSpins();
    void endFreeSpins();
    void startGamble();
    void gamble(bool red);
    void finishGamble();
    void openJackpot(int tier);
    void pickJackpot(int card);
    void gameKey(int key);
    void setWinCounter(double from, double to, float dur);

    void drawGame();
    void drawBackground(const Theme& th, Rect area, float strength);
    void drawReels();
    void drawSymbol(int sym, Rect r, float alpha, float blur, float scale);
    void drawMoneyTag(Rect cell, double value, float alpha, float scale);
    void drawAutoPanel();
    void autoButton();          // butonul AUTO: deschide panoul sau opreste AUTO
    void startAuto(int option);
    void drawCollect();
    float collectFlyStart(int k) const { return 1.45f + k * 0.30f; }
    float collectEnd() const { return collectFlyStart(collectN > 0 ? collectN - 1 : 0) + 0.55f + (collectMult > 1 ? 1.3f : 0.8f); }
    bool collecting() const { return collectT >= 0; }
    void drawWinPresentation();
    void drawLineIndicators();
    void drawPanel();
    void drawFsIntro();
    void drawFsOutro();
    void drawBigWin();
    void drawGamble();
    void drawInfo();
    void drawBuy();
    void drawJackpot();
    void drawCard(Rect r, int suit, int rank, float flip);
    Rect reelWindow() const;
    Rect cellRect(int reel, int row) const;
};

std::string fmtMoney(double v);
float clampf(float v, float a, float b);
float easeOutCubic(float t);
float easeInOut(float t);
float easeOutBack(float t);
Color hexA(uint32_t rgb, float a);
extern Renderer2D* gRenderer;

} // namespace slot
