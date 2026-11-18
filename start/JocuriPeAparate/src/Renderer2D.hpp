#pragma once
// ---------------------------------------------------------------------------
//  Renderer2D - desenare 2D in loturi (batch) cu OpenGL 3.3 core.
//  Un singur shader pentru: dreptunghiuri rotunjite cu degrade si contur,
//  umbre/aure moi, degrade radial, raze rotitoare, model romburi,
//  simboluri din atlas (cu blur vertical pentru role si nuantare),
//  text SDF (contur, umbra, aura, degrade).
//  Coordonatele sunt intr-o panza virtuala de 1280x720 (scalata la textura).
// ---------------------------------------------------------------------------
#include "SlotGL.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace slot {

const float CANVAS_W = 1280.0f;
const float CANVAS_H = 720.0f;

struct Color {
    float r, g, b, a;
};

inline Color rgba(float r, float g, float b, float a = 1.0f) {
    Color c = {r, g, b, a};
    return c;
}
inline Color hexc(uint32_t rgb, float a = 1.0f) {
    return rgba(((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, a);
}
inline Color withA(Color c, float a) {
    c.a = a;
    return c;
}
inline Color scaleA(Color c, float k) {
    c.a *= k;
    return c;
}
inline Color lerpc(Color x, Color y, float t) {
    return rgba(x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t, x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t);
}
inline Color brighten(Color c, float k) {
    return rgba(c.r * k > 1 ? 1 : c.r * k, c.g * k > 1 ? 1 : c.g * k, c.b * k > 1 ? 1 : c.b * k, c.a);
}

struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};
inline Rect R(float x, float y, float w, float h) {
    Rect r = {x, y, w, h};
    return r;
}
inline Rect inset(Rect r, float d) {
    return R(r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d);
}
inline Rect scaled(Rect r, float s) {
    float w = r.w * s, h = r.h * s;
    return R(r.cx() - w * 0.5f, r.cy() - h * 0.5f, w, h);
}
inline Rect centered(float cx, float cy, float w, float h) {
    return R(cx - w * 0.5f, cy - h * 0.5f, w, h);
}

enum FontId { FONT_UI = 0, FONT_DISPLAY = 1, FONT_SERIF = 2, FONT_COUNT = 3 };
enum Align { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 };

struct TextStyle {
    int font;
    float size;
    Color top;           // culoarea de sus (sau unica)
    Color bottom;        // culoarea de jos a degradeului (a = 0 -> la fel ca top)
    float outline;       // grosime contur (px virtuali)
    Color outlineColor;
    float shadow;        // deplasare umbra (px virtuali), 0 = fara
    Color shadowColor;
    float glow;          // raza aurei (px virtuali), 0 = fara
    Color glowColor;
    int align;
    float spacing;       // spatiere suplimentara intre litere (px)

    TextStyle(int f = FONT_UI, float s = 24.0f, Color c = rgba(1, 1, 1, 1))
        : font(f), size(s), top(c), bottom(rgba(0, 0, 0, 0)), outline(0), outlineColor(rgba(0, 0, 0, 1)),
          shadow(0), shadowColor(rgba(0, 0, 0, 0.6f)), glow(0), glowColor(rgba(1, 1, 1, 0.5f)),
          align(ALIGN_LEFT), spacing(0) {}
    TextStyle& grad(Color t, Color b) { top = t; bottom = b; return *this; }
    TextStyle& stroke(float w, Color c) { outline = w; outlineColor = c; return *this; }
    TextStyle& drop(float d, Color c = rgba(0, 0, 0, 0.6f)) { shadow = d; shadowColor = c; return *this; }
    TextStyle& halo(float r, Color c) { glow = r; glowColor = c; return *this; }
    TextStyle& al(int a) { align = a; return *this; }
    TextStyle& sp(float s) { spacing = s; return *this; }
};

// Salveaza / restaureaza starea OpenGL atinsa de interfata,
// ca randarea in textura sa nu strice scena 3D.
struct GLStateBackup {
    GLint fbo, viewport[4], scissorBox[4], program, vao, arrayBuffer, activeTex, tex0, tex1;
    GLint blendSrcRGB, blendDstRGB, blendSrcA, blendDstA, blendEqRGB, blendEqA;
    GLboolean blend, depthTest, cullFace, scissor, stencil, depthMask, srgb;
    GLboolean colorMask[4];
    GLint polygonMode[2];
    GLfloat clearColor[4];
    void save();
    void restore();
};

class Renderer2D {
public:
    Renderer2D();
    ~Renderer2D();

    // Creeaza shaderul si incarca atlasele din assetDir (ex. "JocuriPeAparate/assets/").
    bool init(const std::string& assetDir, std::string* error);
    void shutdown();
    bool ready() const { return program_ != 0; }

    // Cadru nou pe tinta curenta (FBO) de fbW x fbH pixeli. Panza 1280x720 e scalata sa incapa.
    void begin(int fbW, int fbH, float timeSec);
    void end();
    float time() const { return time_; }

    // ---- transformari / decupare / transparenta
    void pushTransform(float tx, float ty, float s);   // p' = p * s + t (relativ la transformarea curenta)
    void pushTransformAround(float cx, float cy, float s, float dx = 0, float dy = 0);
    void popTransform();
    void pushClip(Rect r);
    void popClip();
    void pushAlpha(float a);
    void popAlpha();

    // ---- forme
    void rect(Rect r, Color c, float radius = 0);
    void rectGrad(Rect r, Color top, Color bottom, float radius = 0);
    void rectBorder(Rect r, Color top, Color bottom, float radius, float border, Color borderColor);
    void frame(Rect r, float radius, float thickness, Color c);   // doar conturul
    void softRect(Rect r, float radius, float blur, Color c);     // umbra / aura
    void radial(Rect r, Color inner, Color outer, float radius = 0);
    void rays(Rect r, Color ray, Color base, float count, float phase, float radius = 0);
    void pattern(Rect r, Color base, Color lines, float cell, float radius = 0);
    void circle(float cx, float cy, float rad, Color c);
    void circleGrad(float cx, float cy, float rad, Color top, Color bottom);
    void ring(float cx, float cy, float rad, float thickness, Color c);
    void line(float x0, float y0, float x1, float y1, float thickness, Color c);
    void triangle(float cx, float cy, float size, int dir, Color c);   // sageata: dir 0=stanga 1=dreapta 2=sus 3=jos

    // ---- simboluri din atlas
    void sprite(int id, Rect r, Color mul = rgba(1, 1, 1, 1), float blur = 0, const Color* tint = nullptr);
    void spriteRounded(int id, Rect r, float radius, Color mul = rgba(1, 1, 1, 1));
    // imagine care ocupa cols x rows celule din atlas, incepand cu celula id (ex. SPR_FISHER_BASKET, 2 x 2)
    void spriteBig(int id, int cols, int rows, Rect r, Color mul = rgba(1, 1, 1, 1));

    // ---- text (UTF-8). y = mijlocul inaltimii majusculelor.
    float textWidth(const char* s, int font, float size, float spacing = 0) const;
    void text(const char* s, float x, float y, const TextStyle& st);
    // text incadrat intr-un dreptunghi (aliniere orizontala din stil, centrat vertical, micsorat daca nu incape)
    void textBox(Rect r, const char* s, TextStyle st, float minSize = 8.0f);
    float capHeight(int font) const { return capH_[font]; }

    int drawCalls() const { return drawCalls_; }
    int quadCount() const { return quads_; }

private:
    struct Vertex {
        float x, y, u, v, lx, ly, hx, hy;
        float p0, p1, p2, p3;
        uint8_t c0[4], c1[4], c2[4];
    };
    struct Xf { float tx, ty, s; };
    struct Glyph;

    void quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
              float pad, float mode, float radius, float pa, float pb, Color c0, Color c1, Color c2,
              bool localIsGradient = false, float g0 = 0, float g1 = 0);
    void flush();
    void applyScissor();
    const void* findGlyph(int font, uint32_t cp) const;
    float kerning(int font, uint32_t a, uint32_t b) const;
    void textPass(const char* s, float x, float baseline, const TextStyle& st, int pass);

    GLuint program_, vao_, vbo_, ibo_, symbolTex_, fontTex_;
    GLint uFb_, uTex_, uFont_;
    std::vector<Vertex> verts_;
    std::vector<Xf> xf_;
    std::vector<Rect> clips_;   // in pixeli FBO
    std::vector<float> alpha_;
    int fbW_, fbH_;
    float scale_, offX_, offY_;
    float time_;
    float capH_[FONT_COUNT];
    int asciiIndex_[FONT_COUNT][128];
    int drawCalls_, quads_;
    bool inFrame_;
};

} // namespace slot
