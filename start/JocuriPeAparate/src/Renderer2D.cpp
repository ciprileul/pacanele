#include "Renderer2D.hpp"

#include "FontData.hpp"
#include "PngDecoder.hpp"
#include "SymbolAtlas.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace slot {
namespace {

const int MAX_QUADS = 16384;

// ------------------------------------------------------------------ shadere
const char* kVertexShader = R"GLSL(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec2 aLocal;
layout(location = 3) in vec2 aHalf;
layout(location = 4) in vec4 aParams;
layout(location = 5) in vec4 aC0;
layout(location = 6) in vec4 aC1;
layout(location = 7) in vec4 aC2;
uniform vec2 uFb;
out vec2 vUV;
out vec2 vLocal;
flat out vec2 vHalf;
flat out vec4 vParams;
flat out vec4 vC0;
flat out vec4 vC1;
flat out vec4 vC2;
void main() {
    vUV = aUV;
    vLocal = aLocal;
    vHalf = aHalf;
    vParams = aParams;
    vC0 = aC0;
    vC1 = aC1;
    vC2 = aC2;
    // panza are y in jos; in textura, randul de sus ajunge la v = 1
    gl_Position = vec4(aPos.x / uFb.x * 2.0 - 1.0, 1.0 - aPos.y / uFb.y * 2.0, 0.0, 1.0);
}
)GLSL";

const char* kFragmentShader = R"GLSL(#version 330 core
in vec2 vUV;
in vec2 vLocal;
flat in vec2 vHalf;
flat in vec4 vParams;
flat in vec4 vC0;
flat in vec4 vC1;
flat in vec4 vC2;
uniform sampler2D uTex;
uniform sampler2D uFont;
out vec4 fragColor;

float sdRoundBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float aaMask(float d) {
    float fw = max(fwidth(d), 1e-4);
    return clamp(0.5 - d / fw, 0.0, 1.0);
}
float sdTriangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

void main() {
    int mode = int(vParams.z + 0.5);
    float radius = vParams.x;
    vec4 col = vec4(0.0);
    if (mode == 0) {
        // dreptunghi rotunjit: degrade vertical, contur, margine moale
        float r = min(radius, min(vHalf.x, vHalf.y));
        float d = sdRoundBox(vLocal, vHalf, r);
        float t = clamp((vLocal.y + vHalf.y) / max(2.0 * vHalf.y, 1e-4), 0.0, 1.0);
        vec4 fill = mix(vC0, vC1, t);
        float bw = vParams.w;
        if (bw > 0.0) {
            float fw = max(fwidth(d), 1e-4);
            float inner = clamp(0.5 - (d + bw) / fw, 0.0, 1.0);
            fill = mix(vC2, fill, inner);
        }
        float soft = vParams.y;
        float a = soft > 0.0 ? 1.0 - smoothstep(-soft, soft, d) : aaMask(d);
        col = vec4(fill.rgb, fill.a * a);
    } else if (mode == 1) {
        // simbol din atlas, cu blur vertical optional (role in miscare)
        vec4 t;
        float blur = vParams.y;
        if (blur > 0.0) {
            // vHalf = intervalul v al celulei din atlas (quad-ul e extins pe verticala)
            float v0 = vHalf.x;
            float v1 = vHalf.y;
            t = vec4(0.0);
            float ws = 0.0;
            for (int i = -7; i <= 7; ++i) {
                float w = 1.0 - abs(float(i)) / 8.0;
                float v = vUV.y + float(i) * blur / 7.0;
                vec4 s = (v < v0 || v > v1) ? vec4(0.0) : texture(uTex, vec2(vUV.x, v));
                t += vec4(s.rgb * s.a, s.a) * w;
                ws += w;
            }
            t /= ws;
            t.rgb /= max(t.a, 1e-4);
        } else {
            t = texture(uTex, vUV);
        }
        if (vParams.w > 0.5) {
            // nuanteaza doar partile gri (literele), pastreaza auriul
            float mx = max(t.r, max(t.g, t.b));
            float mn = min(t.r, min(t.g, t.b));
            float k = 1.0 - smoothstep(0.05, 0.16, mx - mn);
            vec3 tinted = t.rgb * vC1.rgb * 1.3 + pow(t.rgb, vec3(5.0)) * 0.55;
            t.rgb = mix(t.rgb, clamp(tinted, 0.0, 1.0), k);
        }
        col = t * vC0;
        if (radius > 0.0) col.a *= aaMask(sdRoundBox(vLocal, vHalf, radius));
    } else if (mode == 2) {
        // text SDF
        float s = texture(uFont, vUV).r;
        float w = max(fwidth(s), 1e-4) * 0.7;
        float soft = vParams.y;
        float edge = 0.5 - vParams.w;
        float ow = vParams.x;
        float fa = smoothstep(edge - w - soft, edge + w + soft, s);
        vec4 fc = mix(vC0, vC1, clamp(vLocal.x, 0.0, 1.0));
        if (ow > 0.0) {
            float oe = edge - ow;
            float oa = smoothstep(oe - w, oe + w, s);
            col = vec4(mix(vC2.rgb, fc.rgb, fa), mix(vC2.a * oa, fc.a, fa));
        } else {
            col = vec4(fc.rgb, fc.a * fa);
        }
    } else if (mode == 3) {
        // degrade radial
        float t = clamp(length(vLocal / max(vHalf, vec2(1e-4))), 0.0, 1.0);
        col = mix(vC0, vC1, t);
        col.a *= aaMask(sdRoundBox(vLocal, vHalf, min(radius, min(vHalf.x, vHalf.y))));
    } else if (mode == 4) {
        // raze rotitoare
        vec2 p = vLocal / max(vHalf.x, vHalf.y);
        float ang = atan(p.y, p.x) + vParams.w;
        float s = 0.5 + 0.5 * cos(ang * vParams.y);
        float ray = smoothstep(0.42, 0.62, s);
        float fade = 1.0 - smoothstep(0.05, 1.05, length(p));
        col = mix(vC1, vC0, ray * fade);
        col.a *= aaMask(sdRoundBox(vLocal, vHalf, min(radius, min(vHalf.x, vHalf.y))));
    } else if (mode == 5) {
        // model de romburi (spatele cartilor, fundaluri)
        vec2 q = vLocal / vParams.y;
        vec2 g = vec2(q.x + q.y, q.x - q.y) * 0.5;
        vec2 f = abs(fract(g) - 0.5);
        float d = min(f.x, f.y);
        float fw = fwidth(d);
        float ln = 1.0 - smoothstep(0.06, 0.06 + fw * 1.5, d);
        col = mix(vC0, vC1, ln);
        col.a *= aaMask(sdRoundBox(vLocal, vHalf, min(radius, min(vHalf.x, vHalf.y))));
    } else if (mode == 6) {
        // triunghi (sageti)
        int dir = int(vParams.w + 0.5);
        vec2 p = vLocal;
        vec2 h = vHalf;
        if (dir == 0) p = vec2(-p.x, p.y);
        else if (dir == 2) { p = vec2(-p.y, p.x); h = h.yx; }
        else if (dir == 3) { p = vec2(p.y, -p.x); h = h.yx; }
        float d = sdTriangle(p, vec2(-h.x, -h.y), vec2(h.x, 0.0), vec2(-h.x, h.y)) - radius;
        col = vec4(vC0.rgb, vC0.a * aaMask(d));
    }
    fragColor = col;
}
)GLSL";

GLuint compileShader(GLenum type, const char* src, std::string* err) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        if (err) *err = std::string("eroare shader: ") + log;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

void pack(uint8_t out[4], Color c, float alphaMul) {
    float v[4] = {c.r, c.g, c.b, c.a * alphaMul};
    for (int i = 0; i < 4; ++i) {
        float x = v[i] < 0.0f ? 0.0f : (v[i] > 1.0f ? 1.0f : v[i]);
        out[i] = (uint8_t)(x * 255.0f + 0.5f);
    }
}

uint32_t decodeUtf8(const char*& p) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
    uint32_t c = s[0];
    int n = 0;
    if (c < 0x80) { n = 1; }
    else if ((c >> 5) == 6 && s[1]) { c = ((c & 31) << 6) | (s[1] & 63); n = 2; }
    else if ((c >> 4) == 14 && s[1] && s[2]) { c = ((c & 15) << 12) | ((s[1] & 63) << 6) | (s[2] & 63); n = 3; }
    else if ((c >> 3) == 30 && s[1] && s[2] && s[3]) { c = ((c & 7) << 18) | ((s[1] & 63) << 12) | ((s[2] & 63) << 6) | (s[3] & 63); n = 4; }
    else { c = '?'; n = 1; }
    p += n;
    return c;
}

const fontdata::Glyph* glyphAt(int font, int idx) {
    return &fontdata::fonts[font].glyphs[idx];
}

} // namespace

// ------------------------------------------------------------------ stare GL
void GLStateBackup::save() {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTex);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex1);
    glActiveTexture((GLenum)activeTex);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcA);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstA);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEqRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEqA);
    blend = glIsEnabled(GL_BLEND);
    depthTest = glIsEnabled(GL_DEPTH_TEST);
    cullFace = glIsEnabled(GL_CULL_FACE);
    scissor = glIsEnabled(GL_SCISSOR_TEST);
    stencil = glIsEnabled(GL_STENCIL_TEST);
    srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    polygonMode[0] = polygonMode[1] = GL_FILL;
    glGetIntegerv(GL_POLYGON_MODE, polygonMode);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
}

static void setCap(GLenum cap, GLboolean on) {
    if (on) glEnable(cap);
    else glDisable(cap);
}

void GLStateBackup::restore() {
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
    glUseProgram((GLuint)program);
    glBindVertexArray((GLuint)vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)arrayBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex1);
    glActiveTexture((GLenum)activeTex);
    glBlendFuncSeparate((GLenum)blendSrcRGB, (GLenum)blendDstRGB, (GLenum)blendSrcA, (GLenum)blendDstA);
    glBlendEquationSeparate((GLenum)blendEqRGB, (GLenum)blendEqA);
    setCap(GL_BLEND, blend);
    setCap(GL_DEPTH_TEST, depthTest);
    setCap(GL_CULL_FACE, cullFace);
    setCap(GL_SCISSOR_TEST, scissor);
    setCap(GL_STENCIL_TEST, stencil);
    setCap(GL_FRAMEBUFFER_SRGB, srgb);
    glDepthMask(depthMask);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)polygonMode[0]);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
}

// ------------------------------------------------------------------ Renderer2D
Renderer2D::Renderer2D()
    : program_(0), vao_(0), vbo_(0), ibo_(0), symbolTex_(0), fontTex_(0), uFb_(-1), uTex_(-1), uFont_(-1),
      fbW_(1), fbH_(1), scale_(1), offX_(0), offY_(0), time_(0), drawCalls_(0), quads_(0), inFrame_(false) {
    for (int f = 0; f < FONT_COUNT; ++f) {
        capH_[f] = 0.7f;
        for (int i = 0; i < 128; ++i) asciiIndex_[f][i] = -1;
    }
}

Renderer2D::~Renderer2D() {}

static GLuint uploadTexture(const PngImage& img, bool mipmaps, int maxLevel) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint oldAlign = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlign);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GLenum fmt = GL_RGBA;
    GLint ifmt = GL_RGBA8;
    if (img.channels == 1) { fmt = GL_RED; ifmt = GL_R8; }
    else if (img.channels == 2) { fmt = GL_RG; ifmt = GL_RG8; }
    else if (img.channels == 3) { fmt = GL_RGB; ifmt = GL_RGB8; }
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, img.width, img.height, 0, fmt, GL_UNSIGNED_BYTE, img.pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlign);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmaps) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxLevel);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

bool Renderer2D::init(const std::string& assetDir, std::string* error) {
    if (program_) return true;
    std::string dir = assetDir;
    if (!dir.empty() && dir[dir.size() - 1] != '/' && dir[dir.size() - 1] != '\\') dir += "/";

    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader, error);
    if (!vs) return false;
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader, error);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        if (error) *error = std::string("eroare link shader: ") + log;
        glDeleteProgram(prog);
        return false;
    }
    program_ = prog;
    uFb_ = glGetUniformLocation(prog, "uFb");
    uTex_ = glGetUniformLocation(prog, "uTex");
    uFont_ = glGetUniformLocation(prog, "uFont");

    GLint prevVao = 0, prevBuf = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevBuf);
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * 4 * 256), nullptr, GL_STREAM_DRAW);
    std::vector<uint32_t> idx((size_t)MAX_QUADS * 6);
    for (uint32_t q = 0; q < (uint32_t)MAX_QUADS; ++q) {
        uint32_t b = q * 4;
        uint32_t* o = &idx[(size_t)q * 6];
        o[0] = b; o[1] = b + 1; o[2] = b + 2; o[3] = b + 2; o[4] = b + 1; o[5] = b + 3;
    }
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);
    const GLsizei st = (GLsizei)sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, st, (const void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (const void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (const void*)offsetof(Vertex, lx));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, st, (const void*)offsetof(Vertex, hx));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, st, (const void*)offsetof(Vertex, p0));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (const void*)offsetof(Vertex, c0));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (const void*)offsetof(Vertex, c1));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (const void*)offsetof(Vertex, c2));
    glBindVertexArray((GLuint)prevVao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prevBuf);

    // atlase
    PngImage img;
    std::string e;
    if (!loadPngFile(dir + "ui_font_sdf.png", img, &e)) {
        if (error) *error = "font: " + e;
        return false;
    }
    fontTex_ = uploadTexture(img, false, 0);
    if (!loadPngFile(dir + "symbols.png", img, &e)) {
        if (error) *error = "simboluri: " + e;
        return false;
    }
    symbolTex_ = uploadTexture(img, true, 5);

    for (int f = 0; f < FONT_COUNT; ++f) {
        const fontdata::Font& fd = fontdata::fonts[f];
        for (int i = 0; i < fd.glyphCount; ++i)
            if (fd.glyphs[i].cp < 128) asciiIndex_[f][fd.glyphs[i].cp] = i;
        int h = asciiIndex_[f]['H'];
        if (h >= 0) capH_[f] = -(fd.glyphs[h].yoff + fd.spread + 1.0f) / fd.base;
    }
    verts_.reserve((size_t)MAX_QUADS * 4);
    return true;
}

void Renderer2D::shutdown() {
    if (program_) glDeleteProgram(program_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ibo_) glDeleteBuffers(1, &ibo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (fontTex_) glDeleteTextures(1, &fontTex_);
    if (symbolTex_) glDeleteTextures(1, &symbolTex_);
    program_ = vao_ = vbo_ = ibo_ = fontTex_ = symbolTex_ = 0;
}

void Renderer2D::begin(int fbW, int fbH, float timeSec) {
    fbW_ = fbW > 0 ? fbW : 1;
    fbH_ = fbH > 0 ? fbH : 1;
    scale_ = std::min(fbW_ / CANVAS_W, fbH_ / CANVAS_H);
    offX_ = (fbW_ - CANVAS_W * scale_) * 0.5f;
    offY_ = (fbH_ - CANVAS_H * scale_) * 0.5f;
    time_ = timeSec;
    xf_.clear();
    Xf id = {0, 0, 1};
    xf_.push_back(id);
    clips_.clear();
    alpha_.clear();
    alpha_.push_back(1.0f);
    verts_.clear();
    drawCalls_ = 0;
    quads_ = 0;
    inFrame_ = true;

    glViewport(0, 0, fbW_, fbH_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(uFb_, (float)fbW_, (float)fbH_);
    glUniform1i(uTex_, 0);
    glUniform1i(uFont_, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, symbolTex_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);
}

void Renderer2D::end() {
    flush();
    glDisable(GL_SCISSOR_TEST);
    inFrame_ = false;
}

void Renderer2D::flush() {
    if (verts_.empty()) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts_.size() * sizeof(Vertex)), verts_.data(), GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)(verts_.size() / 4 * 6), GL_UNSIGNED_INT, (const void*)0);
    ++drawCalls_;
    verts_.clear();
}

// ------------------------------------------------------------------ transformari
void Renderer2D::pushTransform(float tx, float ty, float s) {
    const Xf& c = xf_.back();
    Xf n = {c.s * tx + c.tx, c.s * ty + c.ty, c.s * s};
    xf_.push_back(n);
}

void Renderer2D::pushTransformAround(float cx, float cy, float s, float dx, float dy) {
    pushTransform(cx * (1.0f - s) + dx, cy * (1.0f - s) + dy, s);
}

void Renderer2D::popTransform() {
    if (xf_.size() > 1) xf_.pop_back();
}

void Renderer2D::pushClip(Rect r) {
    flush();
    const Xf& c = xf_.back();
    float x0 = (r.x * c.s + c.tx) * scale_ + offX_;
    float y0 = (r.y * c.s + c.ty) * scale_ + offY_;
    float x1 = (r.right() * c.s + c.tx) * scale_ + offX_;
    float y1 = (r.bottom() * c.s + c.ty) * scale_ + offY_;
    if (!clips_.empty()) {
        const Rect& p = clips_.back();
        x0 = std::max(x0, p.x);
        y0 = std::max(y0, p.y);
        x1 = std::min(x1, p.right());
        y1 = std::min(y1, p.bottom());
    }
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    clips_.push_back(R(x0, y0, x1 - x0, y1 - y0));
    applyScissor();
}

void Renderer2D::popClip() {
    flush();
    if (!clips_.empty()) clips_.pop_back();
    applyScissor();
}

void Renderer2D::applyScissor() {
    if (clips_.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect& c = clips_.back();
    int x = (int)std::floor(c.x), y = (int)std::floor(c.y);
    int x1 = (int)std::ceil(c.right()), y1 = (int)std::ceil(c.bottom());
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, fbH_ - y1, x1 - x, y1 - y);
}

void Renderer2D::pushAlpha(float a) {
    alpha_.push_back(alpha_.back() * a);
}

void Renderer2D::popAlpha() {
    if (alpha_.size() > 1) alpha_.pop_back();
}

// ------------------------------------------------------------------ quad generic
void Renderer2D::quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, float pad,
                      float mode, float radius, float pa, float pb, Color c0, Color c1, Color c2,
                      bool localIsGradient, float g0, float g1) {
    if (verts_.size() / 4 >= (size_t)MAX_QUADS) flush();
    const Xf& t = xf_.back();
    float am = alpha_.back();
    float hw = w * 0.5f, hh = h * 0.5f;
    float X[2] = {x - pad, x + w + pad};
    float Y[2] = {y - pad, y + h + pad};
    float U[2] = {u0, u1};
    float V[2] = {v0, v1};
    float LX[2] = {-hw - pad, hw + pad};
    float LY[2] = {-hh - pad, hh + pad};
    Vertex vx;
    pack(vx.c0, c0, am);
    pack(vx.c1, c1, am);
    pack(vx.c2, c2, am);
    vx.hx = hw;
    vx.hy = hh;
    vx.p0 = radius;
    vx.p1 = pa;
    vx.p2 = mode;
    vx.p3 = pb;
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
            vx.x = (X[i] * t.s + t.tx) * scale_ + offX_;
            vx.y = (Y[j] * t.s + t.ty) * scale_ + offY_;
            vx.u = U[i];
            vx.v = V[j];
            if (localIsGradient) {
                vx.lx = j == 0 ? g0 : g1;
                vx.ly = 0;
            } else {
                vx.lx = LX[i];
                vx.ly = LY[j];
            }
            verts_.push_back(vx);
        }
    }
    ++quads_;
}

// ------------------------------------------------------------------ forme
static Color clear(Color c) {
    return withA(c, 0.0f);
}

void Renderer2D::rect(Rect r, Color c, float radius) {
    float pad = 2.0f / (scale_ * xf_.back().s);
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, pad, 0, radius, 0, 0, c, c, c);
}

void Renderer2D::rectGrad(Rect r, Color top, Color bottom, float radius) {
    float pad = 2.0f / (scale_ * xf_.back().s);
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, pad, 0, radius, 0, 0, top, bottom, top);
}

void Renderer2D::rectBorder(Rect r, Color top, Color bottom, float radius, float border, Color borderColor) {
    float pad = 2.0f / (scale_ * xf_.back().s);
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, pad, 0, radius, 0, border, top, bottom, borderColor);
}

void Renderer2D::frame(Rect r, float radius, float thickness, Color c) {
    float pad = 2.0f / (scale_ * xf_.back().s);
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, pad, 0, radius, 0, thickness, clear(c), clear(c), c);
}

void Renderer2D::softRect(Rect r, float radius, float blur, Color c) {
    float b = std::max(blur, 0.5f);
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, b + 2.0f, 0, radius, b, 0, c, c, c);
}

void Renderer2D::radial(Rect r, Color inner, Color outer, float radius) {
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, 0, 3, radius, 0, 0, inner, outer, inner);
}

void Renderer2D::rays(Rect r, Color ray, Color base, float count, float phase, float radius) {
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, 0, 4, radius, count, phase, ray, base, ray);
}

void Renderer2D::pattern(Rect r, Color base, Color lines, float cell, float radius) {
    quad(r.x, r.y, r.w, r.h, 0, 0, 0, 0, 0, 5, radius, cell, 0, base, lines, base);
}

void Renderer2D::circle(float cx, float cy, float rad, Color c) {
    rect(centered(cx, cy, rad * 2, rad * 2), c, rad);
}

void Renderer2D::circleGrad(float cx, float cy, float rad, Color top, Color bottom) {
    rectGrad(centered(cx, cy, rad * 2, rad * 2), top, bottom, rad);
}

void Renderer2D::ring(float cx, float cy, float rad, float thickness, Color c) {
    frame(centered(cx, cy, rad * 2, rad * 2), rad, thickness, c);
}

void Renderer2D::triangle(float cx, float cy, float size, int dir, Color c) {
    float w = size, h = size;
    if (dir == 0 || dir == 1) w = size * 0.86f;
    else h = size * 0.86f;
    float pad = 2.0f / (scale_ * xf_.back().s);
    quad(cx - w * 0.5f, cy - h * 0.5f, w, h, 0, 0, 0, 0, pad, 6, 0, 0, (float)dir, c, c, c);
}

void Renderer2D::line(float x0, float y0, float x1, float y1, float th, Color c) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    dx /= len;
    dy /= len;
    float nx = -dy, ny = dx;
    float pad = 2.0f / (scale_ * xf_.back().s);
    float hw = len * 0.5f + th * 0.5f, hh = th * 0.5f;
    float ex = hw + pad, ey = hh + pad;
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    if (verts_.size() / 4 >= (size_t)MAX_QUADS) flush();
    const Xf& t = xf_.back();
    Vertex vx;
    float am = alpha_.back();
    pack(vx.c0, c, am);
    pack(vx.c1, c, am);
    pack(vx.c2, c, am);
    vx.hx = hw;
    vx.hy = hh;
    vx.p0 = hh;
    vx.p1 = 0;
    vx.p2 = 0;
    vx.p3 = 0;
    vx.u = vx.v = 0;
    const float sx[4] = {-1, 1, -1, 1};
    const float sy[4] = {-1, -1, 1, 1};
    for (int k = 0; k < 4; ++k) {
        float px = cx + dx * ex * sx[k] + nx * ey * sy[k];
        float py = cy + dy * ex * sx[k] + ny * ey * sy[k];
        vx.x = (px * t.s + t.tx) * scale_ + offX_;
        vx.y = (py * t.s + t.ty) * scale_ + offY_;
        vx.lx = ex * sx[k];
        vx.ly = ey * sy[k];
        verts_.push_back(vx);
    }
    ++quads_;
}

// ------------------------------------------------------------------ simboluri
void Renderer2D::sprite(int id, Rect r, Color mul, float blur, const Color* tint) {
    if (id < 0 || id >= SPR_COUNT) return;
    float cw = 1.0f / SYMBOL_ATLAS_COLS, ch = 1.0f / SYMBOL_ATLAS_ROWS;
    float u0 = (id % SYMBOL_ATLAS_COLS) * cw, v0 = (id / SYMBOL_ATLAS_COLS) * ch;
    Color t = tint ? *tint : rgba(1, 1, 1, 1);
    if (blur <= 0.0f) {
        quad(r.x, r.y, r.w, r.h, u0, v0, u0 + cw, v0 + ch, 0, 1, 0, 0, tint ? 1.0f : 0.0f, mul, t, mul);
        return;
    }
    // blur de miscare: quad-ul se extinde sus/jos, iar shaderul nu citeste in afara celulei
    if (verts_.size() / 4 >= (size_t)MAX_QUADS) flush();
    const Xf& xf = xf_.back();
    float am = alpha_.back();
    float ext = blur * r.h, bv = blur * ch;
    float X[2] = {r.x, r.x + r.w};
    float Y[2] = {r.y - ext, r.y + r.h + ext};
    float U[2] = {u0, u0 + cw};
    float V[2] = {v0 - bv, v0 + ch + bv};
    Vertex vx;
    pack(vx.c0, mul, am);
    pack(vx.c1, t, am);
    pack(vx.c2, mul, am);
    vx.lx = vx.ly = 0;
    vx.hx = v0 + 0.0008f;
    vx.hy = v0 + ch - 0.0008f;
    vx.p0 = 0;
    vx.p1 = bv;
    vx.p2 = 1;
    vx.p3 = tint ? 1.0f : 0.0f;
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            vx.x = (X[i] * xf.s + xf.tx) * scale_ + offX_;
            vx.y = (Y[j] * xf.s + xf.ty) * scale_ + offY_;
            vx.u = U[i];
            vx.v = V[j];
            verts_.push_back(vx);
        }
    ++quads_;
}

void Renderer2D::spriteBig(int id, int cols, int rows, Rect r, Color mul) {
    if (id < 0 || id >= SPR_COUNT) return;
    float cw = 1.0f / SYMBOL_ATLAS_COLS, ch = 1.0f / SYMBOL_ATLAS_ROWS;
    float u0 = (id % SYMBOL_ATLAS_COLS) * cw, v0 = (id / SYMBOL_ATLAS_COLS) * ch;
    quad(r.x, r.y, r.w, r.h, u0, v0, u0 + cw * cols, v0 + ch * rows, 0, 1, 0, 0, 0.0f, mul, mul, mul);
}

void Renderer2D::spriteRounded(int id, Rect r, float radius, Color mul) {
    if (id < 0 || id >= SPR_COUNT) return;
    float cw = 1.0f / SYMBOL_ATLAS_COLS, ch = 1.0f / SYMBOL_ATLAS_ROWS;
    float u0 = (id % SYMBOL_ATLAS_COLS) * cw, v0 = (id / SYMBOL_ATLAS_COLS) * ch;
    quad(r.x, r.y, r.w, r.h, u0, v0, u0 + cw, v0 + ch, 0, 1, radius, 0, 0, mul, mul, mul);
}

// ------------------------------------------------------------------ text
const void* Renderer2D::findGlyph(int font, uint32_t cp) const {
    if (font < 0 || font >= FONT_COUNT) font = 0;
    if (cp < 128) {
        int i = asciiIndex_[font][cp];
        return i >= 0 ? glyphAt(font, i) : nullptr;
    }
    // ş/ţ (sedila) si ș/ț (virgula) sunt ambele in atlas
    const fontdata::Font& fd = fontdata::fonts[font];
    int lo = 0, hi = fd.glyphCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t c = fd.glyphs[mid].cp;
        if (c == cp) return &fd.glyphs[mid];
        if (c < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return nullptr;
}

float Renderer2D::kerning(int font, uint32_t a, uint32_t b) const {
    const fontdata::Font& fd = fontdata::fonts[font];
    int lo = 0, hi = fd.kernCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const fontdata::Kern& k = fd.kerns[mid];
        if (k.a == a && k.b == b) return k.k;
        if (k.a < a || (k.a == a && k.b < b)) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0.0f;
}

float Renderer2D::textWidth(const char* s, int font, float size, float spacing) const {
    if (!s || !*s) return 0.0f;
    if (font < 0 || font >= FONT_COUNT) font = 0;
    const fontdata::Font& fd = fontdata::fonts[font];
    float sc = size / fd.base;
    float w = 0;
    uint32_t prev = 0;
    const char* p = s;
    int n = 0;
    while (*p) {
        uint32_t cp = decodeUtf8(p);
        const fontdata::Glyph* g = static_cast<const fontdata::Glyph*>(findGlyph(font, cp));
        if (!g) g = static_cast<const fontdata::Glyph*>(findGlyph(font, '?'));
        if (!g) continue;
        if (prev) w += kerning(font, prev, cp) * sc;
        w += g->adv * sc;
        prev = cp;
        ++n;
    }
    if (n > 1) w += spacing * (n - 1);
    return w;
}

void Renderer2D::textPass(const char* s, float x, float baseline, const TextStyle& st, int pass) {
    int font = (st.font >= 0 && st.font < FONT_COUNT) ? st.font : 0;
    const fontdata::Font& fd = fontdata::fonts[font];
    float sc = st.size / fd.base;
    float k = fd.base / (st.size * 2.0f * fd.spread);   // unitati SDF per pixel virtual
    float gTop = baseline - capH_[font] * st.size;
    float gBot = baseline + 0.02f * st.size;
    float ow = 0, soft = 0, bias = 0;
    Color c0 = st.top, c1 = st.bottom.a > 0 ? st.bottom : st.top, c2 = st.outlineColor;
    float ox = 0, oy = 0;
    if (pass == 0) {   // aura
        bias = std::min(0.30f, (st.outline + st.glow * 0.3f) * k);
        soft = std::min(0.46f - bias, st.glow * k * 0.8f);
        c0 = c1 = st.glowColor;
    } else if (pass == 1) {   // umbra
        bias = std::min(0.45f, st.outline * k);
        soft = 0.05f;
        c0 = c1 = st.shadowColor;
        ox = st.shadow * 0.6f;
        oy = st.shadow;
    } else {
        ow = std::min(0.45f, st.outline * k);
    }
    float pen = x;
    uint32_t prev = 0;
    const char* p = s;
    float fw = (float)fontdata::ATLAS_W, fh = (float)fontdata::ATLAS_H;
    while (*p) {
        uint32_t cp = decodeUtf8(p);
        const fontdata::Glyph* g = static_cast<const fontdata::Glyph*>(findGlyph(font, cp));
        if (!g) g = static_cast<const fontdata::Glyph*>(findGlyph(font, '?'));
        if (!g) continue;
        if (prev) pen += kerning(font, prev, cp) * sc;
        if (g->w > 0) {
            float gx = pen + g->xoff * sc + ox;
            float gy = baseline + g->yoff * sc + oy;
            float gw = g->w * sc, gh = g->h * sc;
            float t0 = (gy - oy - gTop) / (gBot - gTop);
            float t1 = (gy - oy + gh - gTop) / (gBot - gTop);
            quad(gx, gy, gw, gh, g->x / fw, g->y / fh, (g->x + g->w) / fw, (g->y + g->h) / fh, 0, 2, ow, soft, bias,
                 c0, c1, c2, true, t0, t1);
        }
        pen += g->adv * sc + st.spacing;
        prev = cp;
    }
}

void Renderer2D::text(const char* s, float x, float y, const TextStyle& st) {
    if (!s || !*s) return;
    int font = (st.font >= 0 && st.font < FONT_COUNT) ? st.font : 0;
    float w = textWidth(s, font, st.size, st.spacing);
    float sx = x;
    if (st.align == ALIGN_CENTER) sx = x - w * 0.5f;
    else if (st.align == ALIGN_RIGHT) sx = x - w;
    float baseline = y + capH_[font] * st.size * 0.5f;
    if (st.glow > 0) textPass(s, sx, baseline, st, 0);
    if (st.shadow > 0) textPass(s, sx, baseline, st, 1);
    textPass(s, sx, baseline, st, 2);
}

void Renderer2D::textBox(Rect r, const char* s, TextStyle st, float minSize) {
    if (!s || !*s) return;
    float w = textWidth(s, st.font, st.size, st.spacing);
    if (w > r.w && w > 0) {
        float ns = st.size * r.w / w;
        st.size = ns < minSize ? minSize : ns;
    }
    float x = r.x;
    if (st.align == ALIGN_CENTER) x = r.cx();
    else if (st.align == ALIGN_RIGHT) x = r.right();
    text(s, x, r.cy(), st);
}

} // namespace slot
