#pragma once
// ---------------------------------------------------------------------------
//  SlotEngine - logica pura a pacanelelor (fara OpenGL):
//  benzi de role, linii de plata, wild, scatter, wild extins,
//  rotiri gratuite ("speciale") cu simbol special care se extinde
//  sau cu multiplicator, jackpot comun pe 4 niveluri.
// ---------------------------------------------------------------------------
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#ifndef SLOT_U8
// Pentru siruri cu diacritice in codul sursa (fisierele sunt UTF-8 cu BOM).
#define SLOT_U8(s) reinterpret_cast<const char*>(u8##s)
#endif

namespace slot {

enum SymbolKind {
    KIND_NORMAL = 0,
    KIND_WILD = 1,      // inlocuieste simbolurile normale pe linii
    KIND_SCATTER = 2,   // plateste oriunde pe ecran (x pariul total)
    KIND_BOOK = 3,      // wild + scatter (jocurile cu speciale)
    KIND_MONEY = 4,     // peste cu valoare in lei (nu plateste pe linii)
    KIND_COLLECTOR = 5  // pescarul: in rotirile gratuite strange valorile pestilor
};

enum FreeSpinType {
    FS_NONE = 0,
    FS_EXPANDING = 1,   // un simbol ales devine "speciala" si se extinde pe rola
    FS_MULTIPLIER = 2,  // castigurile din rotirile gratuite sunt multiplicate
    FS_COLLECT = 3      // pescarul strange valorile pestilor de pe ecran
};

struct SymbolDef {
    int sprite;          // SPR_* din SymbolAtlas.hpp
    int kind;            // SymbolKind
    float pay[6];        // pay[n] = multiplicator pentru n simboluri (linie: x pariu/linie, scatter: x pariu total)
    uint32_t tint;       // culoare pentru literele gri (0 = nenuantat)
};

struct Theme {
    uint32_t bgTop, bgBottom;          // fundalul jocului
    uint32_t reelTop, reelBottom;      // fundalul rolelor
    uint32_t frame;                    // rama (auriu, argintiu, neon...)
    uint32_t accent;                   // culoarea de accent (lumini, butoane)
    uint32_t titleTop, titleBottom, titleOutline;
    int titleFont;                     // FONT_DISPLAY sau FONT_SERIF
    int style;                         // efect de fundal: 0 raze, 1 flacari, 2 stele, 3 bule, 4 gheata
    uint32_t tileTop, tileBottom;      // fundalul pictogramei din lobby
};

struct GameDef {
    std::string id;
    std::string name;                  // UTF-8, afisat in lobby si in joc
    int tab;                           // 0 = Clasice, 1 = Speciale
    int reels, rows, lines;
    std::vector<SymbolDef> symbols;
    std::vector<std::vector<int> > weights;   // [rola][simbol] -> din ele se construiesc benzile
    std::vector<std::vector<int> > fsWeights; // benzi separate pentru rotirile gratuite (optional)
    bool expandingWild;                // wild-ul se extinde pe toata rola
    int fsType;                        // FreeSpinType
    int fsCount;                       // cate rotiri gratuite se acorda
    int fsMultiplier;                  // pentru FS_MULTIPLIER
    int scatterMin;                    // cate scatter-e declanseaza speciala
    int buyCost;                       // pretul specialei (x pariul total), 0 = nu se poate cumpara
    float payScale;                    // calibrare RTP (vezi tools/sim.cpp)
    int thumbSprite;                   // simbolul de pe pictograma din lobby
    Theme theme;
};

struct Cell {
    int reel, row;
};

struct WinLine {
    int line;                // indexul liniei; -1 = scatter, -2 = speciala extinsa, -3 = pescarul strange
    int symbol;              // indexul simbolului in GameDef::symbols
    int count;
    double amount;           // in lei
    std::vector<Cell> cells;
};

const int MAX_REELS = 5;
const int MAX_ROWS = 4;

struct SpinResult {
    int stops[MAX_REELS];
    int raw[MAX_REELS][MAX_ROWS];      // simbolurile oprite pe ecran
    int grid[MAX_REELS][MAX_ROWS];     // dupa extinderea wild-ului
    bool wildExpanded[MAX_REELS];
    bool specialExpanded[MAX_REELS];
    float money[MAX_REELS][MAX_ROWS];  // valoarea pestilor (x pariul total), 0 = fara
    bool fsStrips;                      // rezultat de pe benzile rotirilor gratuite
    std::vector<WinLine> wins;
    double total;
    int scatters;
    int fsAwarded;
    SpinResult();
};

// Liniile de plata (randul pe fiecare rola). Jocul foloseste primele GameDef::lines.
const std::vector<std::vector<int> >& paylines(int reels, int rows);

class SlotEngine {
public:
    SlotEngine();
    void seed(unsigned s);
    void setGame(const GameDef* g);
    const GameDef* game() const { return game_; }
    const std::vector<int>& strip(int reel) const { return strips_[reel]; }
    const std::vector<int>& stripFor(int reel, bool fs) const { return (fs && !fsStrips_.empty()) ? fsStrips_[reel] : strips_[reel]; }
    float moneyAt(int reel, int pos, bool fs) const;

    SpinResult spin(double totalBet, bool freeSpin, int specialSymbol);
    SpinResult evaluate(const int* stops, double totalBet, bool freeSpin, int specialSymbol) const;
    // Rezultat ales pentru cumpararea specialei: forteaza 3 scatter-e (se plateste separat pretul).
    SpinResult forcedScatterSpin(double totalBet);
    int pickSpecialSymbol();
    int randInt(int n);
    double rand01();

private:
    const GameDef* game_;
    std::vector<std::vector<int> > strips_, fsStrips_;
    std::vector<std::vector<float> > money_, fsMoney_;
    std::mt19937 rng_;
};

// Construieste banda unei role din ponderi (determinist, fara simboluri identice alaturate).
std::vector<int> buildStrip(const GameDef& g, int reel, uint32_t seed, bool fs = false);

// Jackpot comun tuturor aparatelor (4 niveluri: BRONZ, ARGINT, AUR, DIAMANT).
struct JackpotPool {
    double value[4];
    double hitAt[4];
    bool initialized;
    std::mt19937 rng;
    JackpotPool();
    void init();
    int contribute(double bet);   // intoarce nivelul castigat (0..3) sau -1
    void tick(double dt);         // cresterea "de la ceilalti jucatori"
    double take(int tier);        // plateste si reseteaza nivelul
};
JackpotPool& jackpots();
extern const char* const JACKPOT_NAMES[4];
extern const int JACKPOT_SPRITES[4];

} // namespace slot
