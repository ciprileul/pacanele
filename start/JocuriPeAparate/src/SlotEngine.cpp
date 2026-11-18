#include "SlotEngine.hpp"

#include "SymbolAtlas.hpp"

#include <algorithm>
#include <cmath>

namespace slot {

const char* const JACKPOT_NAMES[4] = {"BRONZ", "ARGINT", "AUR", "DIAMANT"};
const int JACKPOT_SPRITES[4] = {SPR_JP1, SPR_JP2, SPR_JP3, SPR_JP4};

SpinResult::SpinResult() : fsStrips(false), total(0), scatters(0), fsAwarded(0) {
    for (int c = 0; c < MAX_REELS; ++c) {
        stops[c] = 0;
        wildExpanded[c] = false;
        specialExpanded[c] = false;
        for (int r = 0; r < MAX_ROWS; ++r) {
            raw[c][r] = grid[c][r] = 0;
            money[c][r] = 0;
        }
    }
}

// ------------------------------------------------------------------ linii de plata
namespace {

std::vector<std::vector<int> > makeLines5x4() {
    std::vector<std::vector<int> > all;
    std::vector<int> p(5);
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            for (int c = 0; c < 4; ++c)
                for (int d = 0; d < 4; ++d)
                    for (int e = 0; e < 4; ++e) {
                        int v[5] = {a, b, c, d, e};
                        bool ok = true;
                        for (int i = 0; i < 4; ++i)
                            if (std::abs(v[i + 1] - v[i]) > 1) ok = false;
                        if (!ok) continue;
                        all.push_back(std::vector<int>(v, v + 5));
                    }
    struct Scored {
        std::vector<int> l;
        float s;
    };
    std::vector<Scored> sc;
    const int straightOrder[4] = {1, 2, 0, 3};
    for (size_t i = 0; i < all.size(); ++i) {
        const std::vector<int>& l = all[i];
        int steps = 0, changes = 0, prevDir = 0;
        for (int k = 0; k < 4; ++k) {
            int d = l[k + 1] - l[k];
            if (d) {
                ++steps;
                if (prevDir && d != prevDir) ++changes;
                prevDir = d;
            }
        }
        bool sym = l[0] == l[4] && l[1] == l[3];
        float s = steps * 2.0f + changes * 3.0f + (sym ? 0.0f : 4.0f);
        if (steps == 0) {
            for (int k = 0; k < 4; ++k)
                if (straightOrder[k] == l[0]) s = -10.0f + k;
        }
        Scored x = {l, s};
        sc.push_back(x);
    }
    std::stable_sort(sc.begin(), sc.end(), [](const Scored& x, const Scored& y) { return x.s < y.s; });
    std::vector<std::vector<int> > out;
    for (size_t i = 0; i < sc.size(); ++i) out.push_back(sc[i].l);
    return out;
}

std::vector<std::vector<int> > toLines(const int (*rows)[5], int n, int reels) {
    std::vector<std::vector<int> > out;
    for (int i = 0; i < n; ++i) out.push_back(std::vector<int>(rows[i], rows[i] + reels));
    return out;
}

bool isScatterKind(int k) {
    return k == KIND_SCATTER || k == KIND_BOOK;
}

} // namespace

const std::vector<std::vector<int> >& paylines(int reels, int rows) {
    static const int L5x3[20][5] = {
        {1, 1, 1, 1, 1}, {0, 0, 0, 0, 0}, {2, 2, 2, 2, 2}, {0, 1, 2, 1, 0}, {2, 1, 0, 1, 2},
        {1, 0, 0, 0, 1}, {1, 2, 2, 2, 1}, {0, 0, 1, 2, 2}, {2, 2, 1, 0, 0}, {1, 2, 1, 0, 1},
        {1, 0, 1, 2, 1}, {0, 1, 1, 1, 0}, {2, 1, 1, 1, 2}, {0, 1, 0, 1, 0}, {2, 1, 2, 1, 2},
        {1, 1, 0, 1, 1}, {1, 1, 2, 1, 1}, {0, 0, 2, 0, 0}, {2, 2, 0, 2, 2}, {0, 2, 2, 2, 0}};
    static const int L3x3[5][5] = {{1, 1, 1, 0, 0}, {0, 0, 0, 0, 0}, {2, 2, 2, 0, 0}, {0, 1, 2, 0, 0}, {2, 1, 0, 0, 0}};
    static const std::vector<std::vector<int> > a = toLines(L5x3, 20, 5);
    static const std::vector<std::vector<int> > b = toLines(L3x3, 5, 3);
    static const std::vector<std::vector<int> > c = makeLines5x4();
    if (reels == 3) return b;
    if (rows >= 4) return c;
    return a;
}

// ------------------------------------------------------------------ benzi
static int stripViolations(const std::vector<int>& s, const GameDef& g) {
    int n = (int)s.size(), bad = 0;
    for (int i = 0; i < n; ++i) {
        int a = s[i];
        if (s[(i + 1) % n] == a) ++bad;
        if (isScatterKind(g.symbols[a].kind) || g.symbols[a].kind == KIND_WILD) {
            for (int d = 1; d < g.rows; ++d)
                if (s[(i + d) % n] == a) ++bad;
        }
    }
    return bad;
}

std::vector<int> buildStrip(const GameDef& g, int reel, uint32_t seed, bool fs) {
    std::vector<int> pool;
    const std::vector<int>& w = (fs && !g.fsWeights.empty()) ? g.fsWeights[reel] : g.weights[reel];
    for (size_t s = 0; s < w.size(); ++s)
        for (int k = 0; k < w[s]; ++k) pool.push_back((int)s);
    if (pool.size() < 2) return pool;
    std::mt19937 r(seed);
    std::shuffle(pool.begin(), pool.end(), r);
    int bad = stripViolations(pool, g);
    int n = (int)pool.size();
    for (int it = 0; it < 6000 && bad > 0; ++it) {
        int i = (int)(r() % (unsigned)n), j = (int)(r() % (unsigned)n);
        std::swap(pool[i], pool[j]);
        int nb = stripViolations(pool, g);
        if (nb <= bad) bad = nb;
        else std::swap(pool[i], pool[j]);
    }
    return pool;
}

// ------------------------------------------------------------------ motor
SlotEngine::SlotEngine() : game_(nullptr), rng_(std::random_device()()) {}

void SlotEngine::seed(unsigned s) {
    rng_.seed(s);
}

void SlotEngine::setGame(const GameDef* g) {
    game_ = g;
    strips_.clear();
    fsStrips_.clear();
    money_.clear();
    fsMoney_.clear();
    if (!g) return;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < g->id.size(); ++i) h = (h ^ (uint8_t)g->id[i]) * 16777619u;
    for (int c = 0; c < g->reels; ++c) strips_.push_back(buildStrip(*g, c, h + (uint32_t)c * 7919u));
    if (!g->fsWeights.empty())
        for (int c = 0; c < g->reels; ++c) fsStrips_.push_back(buildStrip(*g, c, h + 99991u + (uint32_t)c * 7919u, true));
    // valorile fixe ale pestilor de pe benzi (x pariul total)
    static const float VALS[] = {2, 2, 2, 5, 5, 5, 10, 10, 15, 20, 25, 50};
    std::mt19937 vr(h ^ 0x5bd1e995u);
    for (int pass = 0; pass < 2; ++pass) {
        const std::vector<std::vector<int> >& S = pass ? fsStrips_ : strips_;
        std::vector<std::vector<float> >& M = pass ? fsMoney_ : money_;
        for (size_t c = 0; c < S.size(); ++c) {
            std::vector<float> m(S[c].size(), 0.0f);
            for (size_t i = 0; i < S[c].size(); ++i)
                if (g->symbols[(size_t)S[c][i]].kind == KIND_MONEY) m[i] = VALS[vr() % 12];
            M.push_back(m);
        }
    }
}

float SlotEngine::moneyAt(int reel, int pos, bool fs) const {
    const std::vector<std::vector<float> >& M = (fs && !fsMoney_.empty()) ? fsMoney_ : money_;
    if (reel < 0 || reel >= (int)M.size() || M[(size_t)reel].empty()) return 0.0f;
    int n = (int)M[(size_t)reel].size();
    return M[(size_t)reel][(size_t)(((pos % n) + n) % n)];
}

int SlotEngine::randInt(int n) {
    if (n <= 1) return 0;
    std::uniform_int_distribution<int> d(0, n - 1);
    return d(rng_);
}

double SlotEngine::rand01() {
    std::uniform_real_distribution<double> d(0.0, 1.0);
    return d(rng_);
}

SpinResult SlotEngine::spin(double totalBet, bool freeSpin, int specialSymbol) {
    int stops[MAX_REELS] = {0, 0, 0, 0, 0};
    for (int c = 0; c < game_->reels; ++c) stops[c] = randInt((int)stripFor(c, freeSpin).size());
    return evaluate(stops, totalBet, freeSpin, specialSymbol);
}

SpinResult SlotEngine::forcedScatterSpin(double totalBet) {
    const GameDef& g = *game_;
    int stops[MAX_REELS] = {0, 0, 0, 0, 0};
    for (int c = 0; c < g.reels; ++c) stops[c] = randInt((int)strips_[c].size());
    std::vector<int> order;
    for (int c = 0; c < g.reels; ++c) order.push_back(c);
    std::shuffle(order.begin(), order.end(), rng_);
    int placed = 0;
    for (size_t k = 0; k < order.size() && placed < g.scatterMin; ++k) {
        int c = order[k];
        const std::vector<int>& s = strips_[c];
        std::vector<int> pos;
        for (size_t i = 0; i < s.size(); ++i)
            if (isScatterKind(g.symbols[s[i]].kind)) pos.push_back((int)i);
        if (pos.empty()) continue;
        int p = pos[(size_t)randInt((int)pos.size())];
        int row = randInt(g.rows);
        stops[c] = (p - row + (int)s.size()) % (int)s.size();
        ++placed;
    }
    return evaluate(stops, totalBet, false, -1);
}

int SlotEngine::pickSpecialSymbol() {
    std::vector<int> c;
    for (size_t i = 0; i < game_->symbols.size(); ++i)
        if (game_->symbols[i].kind == KIND_NORMAL) c.push_back((int)i);
    if (c.empty()) return -1;
    return c[(size_t)randInt((int)c.size())];
}

SpinResult SlotEngine::evaluate(const int* stops, double totalBet, bool freeSpin, int special) const {
    const GameDef& g = *game_;
    SpinResult r;
    r.fsStrips = freeSpin && !fsStrips_.empty();
    for (int c = 0; c < g.reels; ++c) {
        const std::vector<int>& s = stripFor(c, freeSpin);
        int n = (int)s.size();
        r.stops[c] = stops[c];
        for (int row = 0; row < g.rows; ++row) {
            r.raw[c][row] = r.grid[c][row] = s[(stops[c] + row) % n];
            r.money[c][row] = moneyAt(c, stops[c] + row, freeSpin);
        }
    }
    // wild extins
    if (g.expandingWild) {
        for (int c = 0; c < g.reels; ++c) {
            int wsym = -1;
            for (int row = 0; row < g.rows; ++row)
                if (g.symbols[r.raw[c][row]].kind == KIND_WILD) wsym = r.raw[c][row];
            if (wsym >= 0) {
                r.wildExpanded[c] = true;
                for (int row = 0; row < g.rows; ++row) r.grid[c][row] = wsym;
            }
        }
    }
    const double lineBet = totalBet / g.lines;
    const double scale = g.payScale;
    const std::vector<std::vector<int> >& lines = paylines(g.reels, g.rows);
    int nl = std::min(g.lines, (int)lines.size());
    for (int l = 0; l < nl; ++l) {
        const std::vector<int>& p = lines[(size_t)l];
        int first = -1, count = 0;
        for (int c = 0; c < g.reels; ++c) {
            int s = r.grid[c][p[(size_t)c]];
            int k = g.symbols[s].kind;
            if (k == KIND_SCATTER || k == KIND_MONEY || k == KIND_COLLECTOR) break;
            if (k == KIND_WILD || k == KIND_BOOK) {
                ++count;
                continue;
            }
            if (first < 0) {
                first = s;
                ++count;
            } else if (s == first) {
                ++count;
            } else {
                break;
            }
        }
        int wcount = 0, wsym = -1;
        for (int c = 0; c < g.reels; ++c) {
            int s = r.grid[c][p[(size_t)c]];
            if (g.symbols[s].kind != KIND_WILD) break;
            ++wcount;
            wsym = s;
        }
        double payMain = first >= 0 ? g.symbols[first].pay[count] : 0.0;
        double payWild = wsym >= 0 ? g.symbols[wsym].pay[wcount] : 0.0;
        if (payMain <= 0 && payWild <= 0) continue;
        WinLine w;
        w.line = l;
        if (payWild > payMain) {
            w.symbol = wsym;
            w.count = wcount;
            w.amount = payWild * lineBet * scale;
        } else {
            w.symbol = first;
            w.count = count;
            w.amount = payMain * lineBet * scale;
        }
        for (int c = 0; c < w.count; ++c) {
            Cell cell = {c, p[(size_t)c]};
            w.cells.push_back(cell);
        }
        r.wins.push_back(w);
    }
    // scatter (oriunde pe ecran)
    for (size_t s = 0; s < g.symbols.size(); ++s) {
        if (!isScatterKind(g.symbols[s].kind)) continue;
        WinLine w;
        w.line = -1;
        w.symbol = (int)s;
        w.count = 0;
        for (int c = 0; c < g.reels; ++c)
            for (int row = 0; row < g.rows; ++row)
                if (r.raw[c][row] == (int)s) {
                    Cell cell = {c, row};
                    w.cells.push_back(cell);
                    ++w.count;
                }
        r.scatters += w.count;
        int cnt = std::min(w.count, 5);
        if (g.symbols[s].pay[cnt] > 0) {
            w.amount = g.symbols[s].pay[cnt] * totalBet * scale;
            r.wins.push_back(w);
        }
    }
    if (g.fsType != FS_NONE && r.scatters >= g.scatterMin) r.fsAwarded = g.fsCount;
    // simbolul special care se extinde (doar in rotirile speciale)
    if (freeSpin && g.fsType == FS_EXPANDING && special >= 0) {
        int reelsWith = 0;
        bool has[MAX_REELS] = {false, false, false, false, false};
        for (int c = 0; c < g.reels; ++c)
            for (int row = 0; row < g.rows; ++row)
                if (r.raw[c][row] == special) has[c] = true;
        for (int c = 0; c < g.reels; ++c)
            if (has[c]) ++reelsWith;
        if (g.symbols[(size_t)special].pay[reelsWith] > 0) {
            WinLine w;
            w.line = -2;
            w.symbol = special;
            w.count = reelsWith;
            w.amount = g.symbols[(size_t)special].pay[reelsWith] * lineBet * nl * scale;
            for (int c = 0; c < g.reels; ++c) {
                if (!has[c]) continue;
                r.specialExpanded[c] = true;
                for (int row = 0; row < g.rows; ++row) {
                    Cell cell = {c, row};
                    w.cells.push_back(cell);
                }
            }
            r.wins.push_back(w);
        }
    }
    // pescarul strange valorile tuturor pestilor de pe ecran (doar in rotirile gratuite)
    if (freeSpin && g.fsType == FS_COLLECT) {
        WinLine w;
        w.line = -3;
        w.symbol = -1;
        w.count = 0;
        double sum = 0;
        int collectors = 0;
        for (int c = 0; c < g.reels; ++c)
            for (int row = 0; row < g.rows; ++row) {
                int k = g.symbols[(size_t)r.raw[c][row]].kind;
                if (k == KIND_COLLECTOR) {
                    ++collectors;
                    w.symbol = r.raw[c][row];
                } else if (k == KIND_MONEY) {
                    sum += r.money[c][row];
                }
            }
        if (collectors > 0 && sum > 0) {
            for (int c = 0; c < g.reels; ++c)
                for (int row = 0; row < g.rows; ++row) {
                    int k = g.symbols[(size_t)r.raw[c][row]].kind;
                    if (k == KIND_COLLECTOR || k == KIND_MONEY) {
                        Cell cell = {c, row};
                        w.cells.push_back(cell);
                    }
                }
            w.count = collectors;
            w.amount = sum * collectors * totalBet;   // exact valorile afisate pe pesti
            r.wins.push_back(w);
        }
    }
    if (freeSpin && g.fsType == FS_MULTIPLIER && g.fsMultiplier > 1) {
        for (size_t i = 0; i < r.wins.size(); ++i) r.wins[i].amount *= g.fsMultiplier;
    }
    r.total = 0;
    for (size_t i = 0; i < r.wins.size(); ++i) {
        r.wins[i].amount = std::floor(r.wins[i].amount * 100.0 + 0.5) / 100.0;
        r.total += r.wins[i].amount;
    }
    return r;
}

// ------------------------------------------------------------------ jackpot
namespace {
const double JP_SEED[4] = {10.0, 100.0, 500.0, 2500.0};
const double JP_RANGE[4] = {25.0, 150.0, 1000.0, 7500.0};
// Sansa de jackpot la fiecare rotire (independenta de miza). La ~3 s pe rotire:
// BRONZ ~ o data la 7,5 ore, ARGINT ~ 33 ore, AUR ~ 125 ore, DIAMANT ~ 500 ore,
// adica in total un jackpot cam o data la 5,5 - 6 ore de joc continuu.
const double JP_CHANCE[4] = {1.0 / 9000.0, 1.0 / 40000.0, 1.0 / 150000.0, 1.0 / 600000.0};
// cresterea afisata (lei per 1 leu pariat), potrivita cu cat de rar iese fiecare nivel
const double JP_RATE[4] = {0.0028, 0.0037, 0.0067, 0.0125};
const double JP_AMBIENT[4] = {0.0009, 0.0012, 0.0022, 0.0042};   // "ceilalti jucatori", lei pe secunda
} // namespace

JackpotPool::JackpotPool() : initialized(false) {
    for (int t = 0; t < 4; ++t) value[t] = hitAt[t] = 0;
}

void JackpotPool::init() {
    if (initialized) return;
    rng.seed(std::random_device()());
    std::uniform_real_distribution<double> u(0.1, 1.0), v(0.05, 0.8);
    for (int t = 0; t < 4; ++t) {
        hitAt[t] = JP_SEED[t] + u(rng) * JP_RANGE[t];
        value[t] = JP_SEED[t] + v(rng) * (hitAt[t] - JP_SEED[t]);
    }
    initialized = true;
}

int JackpotPool::contribute(double bet) {
    init();
    for (int t = 0; t < 4; ++t) value[t] = std::min(value[t] + bet * JP_RATE[t], JP_SEED[t] + 2.0 * JP_RANGE[t]);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    double r = u(rng), acc = 0.0;
    for (int t = 3; t >= 0; --t) {          // un singur nivel pe rotire, cel mai mare are prioritate
        acc += JP_CHANCE[t];
        if (r < acc) return t;
    }
    return -1;
}

void JackpotPool::tick(double dt) {
    init();
    for (int t = 0; t < 4; ++t) {
        double limit = JP_SEED[t] + 2.0 * JP_RANGE[t];
        if (value[t] < limit) value[t] = std::min(limit, value[t] + JP_AMBIENT[t] * dt);
    }
}

double JackpotPool::take(int tier) {
    init();
    if (tier < 0 || tier > 3) return 0;
    double amount = std::floor(value[tier] * 100.0 + 0.5) / 100.0;
    std::uniform_real_distribution<double> u(0.1, 1.0);
    value[tier] = JP_SEED[tier];
    hitAt[tier] = JP_SEED[tier] + u(rng) * JP_RANGE[tier];
    return amount;
}

JackpotPool& jackpots() {
    static JackpotPool pool;
    return pool;
}

} // namespace slot
