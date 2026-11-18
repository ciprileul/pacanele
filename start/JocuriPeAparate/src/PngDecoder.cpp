// Decodor PNG + inflate (zlib) scris de la zero pentru acest proiect.
#include "PngDecoder.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>

namespace slot {
namespace {

// ------------------------------------------------------------------ citire biti
struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
    uint32_t buf;
    int count;
    int overrun;

    BitReader(const uint8_t* d, size_t n) : data(d), size(n), pos(0), buf(0), count(0), overrun(0) {}

    void fill() {
        while (count <= 24) {
            uint32_t byte = 0;
            if (pos < size) byte = data[pos];
            else ++overrun;
            ++pos;
            buf |= byte << count;
            count += 8;
        }
    }
    uint32_t bits(int n) {
        if (n == 0) return 0;
        if (count < n) fill();
        uint32_t v = buf & ((1u << n) - 1u);
        buf >>= n;
        count -= n;
        return v;
    }
};

int reverseBits(int v, int n) {
    int r = 0;
    for (int i = 0; i < n; ++i) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}

// ------------------------------------------------------------------ Huffman
const int FAST_BITS = 9;
const int FAST_SIZE = 1 << FAST_BITS;

struct Huffman {
    uint16_t fast[FAST_SIZE];
    int firstCode[17];
    int maxCode[18];
    int firstSym[17];
    uint8_t size[288];
    uint16_t value[288];

    bool build(const uint8_t* lengths, int num) {
        int cnt[17];
        std::memset(cnt, 0, sizeof(cnt));
        std::memset(fast, 0, sizeof(fast));
        for (int i = 0; i < num; ++i) cnt[lengths[i]]++;
        cnt[0] = 0;
        for (int i = 1; i < 16; ++i)
            if (cnt[i] > (1 << i)) return false;
        int next[17];
        int code = 0, k = 0;
        for (int i = 1; i < 16; ++i) {
            next[i] = code;
            firstCode[i] = code;
            firstSym[i] = k;
            code += cnt[i];
            if (cnt[i] && code - 1 >= (1 << i)) return false;
            maxCode[i] = code << (16 - i);
            code <<= 1;
            k += cnt[i];
        }
        maxCode[16] = 0x10000;
        for (int i = 0; i < num; ++i) {
            int s = lengths[i];
            if (!s) continue;
            int c = next[s] - firstCode[s] + firstSym[s];
            size[c] = (uint8_t)s;
            value[c] = (uint16_t)i;
            if (s <= FAST_BITS) {
                uint16_t fv = (uint16_t)((s << 9) | i);
                int j = reverseBits(next[s], s);
                while (j < FAST_SIZE) {
                    fast[j] = fv;
                    j += (1 << s);
                }
            }
            next[s]++;
        }
        return true;
    }

    int decode(BitReader& br) const {
        if (br.count < 16) br.fill();
        int b = fast[br.buf & (FAST_SIZE - 1)];
        if (b) {
            int s = b >> 9;
            br.buf >>= s;
            br.count -= s;
            return b & 511;
        }
        int k = reverseBits((int)(br.buf & 0xFFFF), 16);
        int s = FAST_BITS + 1;
        for (; s < 17; ++s)
            if (k < maxCode[s]) break;
        if (s >= 16) return -1;
        int idx = (k >> (16 - s)) - firstCode[s] + firstSym[s];
        if (idx < 0 || idx >= 288 || size[idx] != s) return -1;
        br.buf >>= s;
        br.count -= s;
        return value[idx];
    }
};

const int LEN_BASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                          35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int LEN_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                           3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int DIST_BASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                           257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                            7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

struct OutBuf {
    std::vector<uint8_t>& v;
    size_t pos;
    explicit OutBuf(std::vector<uint8_t>& vec) : v(vec), pos(0) {}
    void ensure(size_t extra) {
        if (pos + extra > v.size()) v.resize((pos + extra) * 2);
    }
};

bool inflateBlock(BitReader& br, const Huffman& lit, const Huffman& dist, OutBuf& out) {
    for (;;) {
        int sym = lit.decode(br);
        if (sym < 0) return false;
        if (sym < 256) {
            out.ensure(1);
            out.v[out.pos++] = (uint8_t)sym;
            continue;
        }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) return false;
        int len = LEN_BASE[sym] + (int)br.bits(LEN_EXTRA[sym]);
        int ds = dist.decode(br);
        if (ds < 0 || ds >= 30) return false;
        int d = DIST_BASE[ds] + (int)br.bits(DIST_EXTRA[ds]);
        if ((size_t)d > out.pos) return false;
        out.ensure((size_t)len);
        uint8_t* o = out.v.data() + out.pos;
        const uint8_t* s = o - d;
        for (int i = 0; i < len; ++i) o[i] = s[i];
        out.pos += (size_t)len;
        if (br.overrun > 8) return false;
    }
}

bool zlibInflate(const uint8_t* in, size_t n, std::vector<uint8_t>& vec, size_t expected) {
    if (n < 2) return false;
    int cmf = in[0], flg = in[1];
    if ((cmf * 256 + flg) % 31 != 0) return false;
    if ((cmf & 15) != 8) return false;
    if (flg & 32) return false;
    vec.assign(expected + 64, 0);
    OutBuf out(vec);
    BitReader br(in + 2, n - 2);
    Huffman lit, dist;
    int final = 0;
    do {
        final = (int)br.bits(1);
        int type = (int)br.bits(2);
        if (type == 0) {
            br.bits(br.count & 7);
            uint32_t len = br.bits(16);
            uint32_t nlen = br.bits(16);
            if ((len ^ 0xFFFF) != nlen) return false;
            out.ensure(len);
            for (uint32_t i = 0; i < len; ++i) out.v[out.pos++] = (uint8_t)br.bits(8);
            if (br.overrun > 4) return false;
        } else if (type == 1) {
            uint8_t l[288];
            int i = 0;
            for (; i < 144; ++i) l[i] = 8;
            for (; i < 256; ++i) l[i] = 9;
            for (; i < 280; ++i) l[i] = 7;
            for (; i < 288; ++i) l[i] = 8;
            uint8_t dl[30];
            for (int j = 0; j < 30; ++j) dl[j] = 5;
            if (!lit.build(l, 288) || !dist.build(dl, 30)) return false;
            if (!inflateBlock(br, lit, dist, out)) return false;
        } else if (type == 2) {
            int hlit = (int)br.bits(5) + 257;
            int hdist = (int)br.bits(5) + 1;
            int hclen = (int)br.bits(4) + 4;
            static const int order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t cl[19];
            std::memset(cl, 0, sizeof(cl));
            for (int i = 0; i < hclen; ++i) cl[order[i]] = (uint8_t)br.bits(3);
            Huffman clh;
            if (!clh.build(cl, 19)) return false;
            uint8_t lens[288 + 32];
            int total = hlit + hdist, k = 0;
            while (k < total) {
                int c = clh.decode(br);
                if (c < 0) return false;
                if (c < 16) {
                    lens[k++] = (uint8_t)c;
                } else {
                    int rep = 0;
                    uint8_t val = 0;
                    if (c == 16) {
                        if (k == 0) return false;
                        rep = 3 + (int)br.bits(2);
                        val = lens[k - 1];
                    } else if (c == 17) {
                        rep = 3 + (int)br.bits(3);
                    } else {
                        rep = 11 + (int)br.bits(7);
                    }
                    if (k + rep > total) return false;
                    for (int r = 0; r < rep; ++r) lens[k++] = val;
                }
            }
            if (!lit.build(lens, hlit) || !dist.build(lens + hlit, hdist)) return false;
            if (!inflateBlock(br, lit, dist, out)) return false;
        } else {
            return false;
        }
    } while (!final);
    vec.resize(out.pos);
    return true;
}

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

bool fail(std::string* e, const char* msg) {
    if (e) *e = msg;
    return false;
}

} // namespace

bool decodePng(const unsigned char* data, size_t size, PngImage& out, std::string* error) {
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || std::memcmp(data, sig, 8) != 0) return fail(error, "nu este fisier PNG");
    size_t p = 8;
    uint32_t w = 0, h = 0;
    int depth = 0, ctype = -1, interlace = 0;
    std::vector<uint8_t> idat;
    std::vector<uint8_t> palette;
    std::vector<uint8_t> trns;
    while (p + 8 <= size) {
        uint32_t len = be32(data + p);
        const uint8_t* type = data + p + 4;
        const uint8_t* body = data + p + 8;
        if (p + 12 + (size_t)len > size) return fail(error, "PNG trunchiat");
        if (!std::memcmp(type, "IHDR", 4)) {
            if (len < 13) return fail(error, "IHDR invalid");
            w = be32(body);
            h = be32(body + 4);
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
        } else if (!std::memcmp(type, "PLTE", 4)) {
            palette.assign(body, body + len);
        } else if (!std::memcmp(type, "tRNS", 4)) {
            trns.assign(body, body + len);
        } else if (!std::memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), body, body + len);
        } else if (!std::memcmp(type, "IEND", 4)) {
            break;
        }
        p += 12 + (size_t)len;
    }
    if (w == 0 || h == 0 || w > 16384 || h > 16384) return fail(error, "dimensiuni PNG invalide");
    if (interlace) return fail(error, "PNG intretesut (interlaced) nesuportat - salveaza fara interlace");
    int srcCh = 0;
    switch (ctype) {
        case 0: srcCh = 1; break;
        case 2: srcCh = 3; break;
        case 3: srcCh = 1; break;
        case 4: srcCh = 2; break;
        case 6: srcCh = 4; break;
        default: return fail(error, "tip de culoare PNG necunoscut");
    }
    if (!(depth == 8 || (depth == 16 && ctype != 3))) return fail(error, "PNG trebuie sa fie pe 8 sau 16 biti");
    int bpc = depth / 8;
    size_t bpp = (size_t)srcCh * (size_t)bpc;
    size_t stride = (size_t)w * bpp;
    std::vector<uint8_t> raw;
    if (!zlibInflate(idat.data(), idat.size(), raw, (stride + 1) * h)) return fail(error, "date PNG corupte (inflate)");
    if (raw.size() < (stride + 1) * h) return fail(error, "date PNG incomplete");

    std::vector<uint8_t> img(stride * h);
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* line = raw.data() + (size_t)y * (stride + 1);
        int filter = line[0];
        ++line;
        uint8_t* cur = img.data() + (size_t)y * stride;
        const uint8_t* prev = y ? cur - stride : nullptr;
        for (size_t x = 0; x < stride; ++x) {
            int a = x >= bpp ? cur[x - bpp] : 0;
            int b = prev ? prev[x] : 0;
            int c = (prev && x >= bpp) ? prev[x - bpp] : 0;
            int v = line[x];
            switch (filter) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) >> 1; break;
                case 4: v += paeth(a, b, c); break;
                default: return fail(error, "filtru PNG invalid");
            }
            cur[x] = (uint8_t)v;
        }
    }

    out.width = (int)w;
    out.height = (int)h;
    if (ctype == 3) {
        bool alpha = !trns.empty();
        out.channels = alpha ? 4 : 3;
        out.pixels.resize((size_t)w * h * (size_t)out.channels);
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            int idx = img[i];
            uint8_t* o = &out.pixels[i * (size_t)out.channels];
            o[0] = (size_t)idx * 3 + 2 < palette.size() ? palette[(size_t)idx * 3 + 0] : 0;
            o[1] = (size_t)idx * 3 + 2 < palette.size() ? palette[(size_t)idx * 3 + 1] : 0;
            o[2] = (size_t)idx * 3 + 2 < palette.size() ? palette[(size_t)idx * 3 + 2] : 0;
            if (alpha) o[3] = (size_t)idx < trns.size() ? trns[(size_t)idx] : 255;
        }
        return true;
    }
    out.channels = srcCh;
    if (bpc == 1) {
        out.pixels.swap(img);
    } else {
        out.pixels.resize((size_t)w * h * (size_t)srcCh);
        for (size_t i = 0; i < out.pixels.size(); ++i) out.pixels[i] = img[i * 2];
    }
    return true;
}

bool loadPngFile(const std::string& path, PngImage& out, std::string* error) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) {
        if (error) *error = "nu pot deschide " + path;
        return false;
    }
    std::vector<unsigned char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string e;
    if (!decodePng(buf.data(), buf.size(), out, &e)) {
        if (error) *error = path + ": " + e;
        return false;
    }
    return true;
}

} // namespace slot
