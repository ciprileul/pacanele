#include "CasinoScene.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace cazino {

namespace {

// ------------------------------------------------------------ matrice 4x4 (coloane)
struct Mat {
    float m[16];
};
Mat ident() {
    Mat r;
    std::memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}
Mat mul(const Mat& a, const Mat& b) {
    Mat r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
Mat trans(float x, float y, float z) {
    Mat r = ident();
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}
Mat rotY(float a) {
    Mat r = ident();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[8] = s; r.m[2] = -s; r.m[10] = c;
    return r;
}
Mat rotX(float a) {
    Mat r = ident();
    float c = std::cos(a), s = std::sin(a);
    r.m[5] = c; r.m[9] = -s; r.m[6] = s; r.m[10] = c;
    return r;
}

// ------------------------------------------------------------ asezarea in scena
const float MACHINES_X[3] = {-0.80f, 0.0f, 0.80f};
const float MACHINES_Z = -2.2f;
const float BJ_POS[3] = {-2.6f, 0.0f, 0.2f}, BJ_ROT = 0.55f;
const float RL_POS[3] = {2.7f, 0.0f, 0.1f}, RL_ROT = -0.55f;
const float WHEEL_CENTER[3] = {-0.86f, 0.82f, 0.0f};   // in coordonatele ruleta.obj (vezi tools/gen_models.py)
const int INTERACTIVE = 1;                              // aparatul din mijloc

const int NUM_LIGHTS = 5;
const float LIGHT_POS[NUM_LIGHTS * 3] = {
    0.0f, 2.9f, -1.0f,     // deasupra aparatelor
    -2.6f, 2.6f, 0.2f,     // blackjack
    2.7f, 2.6f, 0.1f,      // ruleta
    0.0f, 2.8f, 2.0f,      // sala
    -1.2f, 1.2f, -1.4f,    // lumina slaba de umplere
};
const float LIGHT_COL[NUM_LIGHTS * 3] = {
    1.5f, 1.35f, 1.2f,
    1.6f, 1.4f, 1.15f,
    1.6f, 1.4f, 1.15f,
    0.9f, 0.8f, 0.75f,
    0.25f, 0.22f, 0.35f,
};

const char* kVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWorld;
out vec3 vN;
out vec2 vUV;
void main() {
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vN = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uViewProj * w;
}
)";

const char* kFS = R"(#version 330 core
in vec3 vWorld;
in vec3 vN;
in vec2 vUV;
uniform vec3 uKd;
uniform vec3 uKs;
uniform float uNs;
uniform vec3 uKe;
uniform int uUseTex;
uniform int uUseEmissiveTex;
uniform sampler2D uTexKd;
uniform sampler2D uTexKe;
uniform vec3 uCam;
uniform vec3 uLightPos[5];
uniform vec3 uLightCol[5];
uniform vec3 uAmb;
out vec4 fragColor;
void main() {
    vec3 base = uKd;
    if (uUseTex == 1) base *= texture(uTexKd, vUV).rgb;
    vec3 N = normalize(vN);
    vec3 V = normalize(uCam - vWorld);
    if (dot(N, V) < 0.0) N = -N;               // fete vazute din spate
    vec3 col = uAmb * base;
    for (int i = 0; i < 5; ++i) {
        vec3 Lv = uLightPos[i] - vWorld;
        float dist = length(Lv);
        vec3 L = Lv / dist;
        float att = 1.0 / (1.0 + 0.09 * dist + 0.06 * dist * dist);
        float diff = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float spec = diff > 0.0 ? pow(max(dot(N, H), 0.0), max(uNs, 1.0)) : 0.0;
        col += (base * diff + uKs * spec) * uLightCol[i] * att;
    }
    vec3 em = uKe;
    if (uUseEmissiveTex == 1) em *= texture(uTexKe, vUV).rgb;
    col += em * 1.5;
    col = vec3(1.0) - exp(-col * 1.25);        // expunere: luminile puternice nu se ard
    fragColor = vec4(col, 1.0);
}
)";

GLuint compile(GLenum t, const char* s, std::string& err) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        err = log;
    }
    return sh;
}

} // namespace

bool CasinoScene::init(const std::string& dir) {
    release();
    struct {
        ObjModel* m;
        const char* file;
    } list[] = {{&slot_, "aparat.obj"}, {&blackjack_, "blackjack.obj"}, {&roulette_, "ruleta.obj"}, {&floor_, "podea.obj"}};
    for (auto& e : list)
        if (!e.m->load(dir + e.file)) {
            error_ = e.m->error();
            return false;
        }
    std::string err;
    GLuint vs = compile(GL_VERTEX_SHADER, kVS, err), fs = compile(GL_FRAGMENT_SHADER, kFS, err);
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);
    glLinkProgram(prog_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok || !err.empty()) {
        char log[2048] = {0};
        glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
        error_ = "shader: " + err + log;
        return false;
    }
    u_ = ObjUniforms::fromProgram(prog_);
    uModel_ = glGetUniformLocation(prog_, "uModel");
    uViewProj_ = glGetUniformLocation(prog_, "uViewProj");
    uCam_ = glGetUniformLocation(prog_, "uCam");
    uLightPos_ = glGetUniformLocation(prog_, "uLightPos");
    uLightCol_ = glGetUniformLocation(prog_, "uLightCol");
    uAmb_ = glGetUniformLocation(prog_, "uAmb");
    uTexKd_ = glGetUniformLocation(prog_, "uTexKd");
    uTexKe_ = glGetUniformLocation(prog_, "uTexKe");

    // ecranele aparatului, din cutiile obiectelor "ecran" si "ecran_sus"
    Mat machine = trans(MACHINES_X[INTERACTIVE], 0, MACHINES_Z);
    for (int k = 0; k < 2; ++k) {
        int e = slot_.findObject(k == 0 ? "ecran" : "ecran_sus");
        float cx = 0, cy = k == 0 ? 1.40f : 1.80f, cz = 0.2f, tilt = 0.12f, sw = 0.6f, sh = 0.3375f;
        if (e >= 0) {
            const ObjObject& o = slot_.objects()[(size_t)e];
            cx = 0.5f * (o.bmin[0] + o.bmax[0]);
            cy = 0.5f * (o.bmin[1] + o.bmax[1]);
            cz = 0.5f * (o.bmin[2] + o.bmax[2]);
            float dy = o.bmax[1] - o.bmin[1], dz = o.bmax[2] - o.bmin[2];
            tilt = std::atan2(dz, dy);
            sw = o.bmax[0] - o.bmin[0];
            sh = std::sqrt(dy * dy + dz * dz);
        }
        Mat sm = mul(machine, mul(trans(cx, cy, cz + 0.0015f), rotX(-tilt)));
        if (k == 0) {
            std::memcpy(screenLocal_, sm.m, sizeof(sm.m));
            screenW_ = sw;
            screenH_ = sh;
        } else {
            std::memcpy(topLocal_, sm.m, sizeof(sm.m));
            topW_ = sw;
            topH_ = sh;
        }
    }

    // quad pentru ecranul viu (pozitie, normala, uv)
    float w = screenW_ * 0.5f, h = screenH_ * 0.5f;
    float q[6 * 8] = {-w, -h, 0, 0, 0, 1, 0, 0, w, -h, 0, 0, 0, 1, 1, 0, w, h, 0, 0, 0, 1, 1, 1,
                      -w, -h, 0, 0, 0, 1, 0, 0, w, h, 0, 0, 0, 1, 1, 1, -w, h, 0, 0, 0, 1, 0, 1};
    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
    for (int a = 0; a < 3; ++a) glEnableVertexAttribArray((GLuint)a);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void*)12);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void*)24);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void CasinoScene::release() {
    slot_.release();
    blackjack_.release();
    roulette_.release();
    floor_.release();
    if (prog_) glDeleteProgram(prog_);
    if (quadVbo_) glDeleteBuffers(1, &quadVbo_);
    if (quadVao_) glDeleteVertexArrays(1, &quadVao_);
    prog_ = quadVao_ = quadVbo_ = 0;
}

void CasinoScene::screenMatrix(float out[16]) const { std::memcpy(out, screenLocal_, sizeof(screenLocal_)); }

void CasinoScene::machinePos(float out[3]) {
    out[0] = MACHINES_X[INTERACTIVE]; out[1] = 0; out[2] = MACHINES_Z;
}
void CasinoScene::blackjackPos(float out[3]) { std::memcpy(out, BJ_POS, sizeof(BJ_POS)); }
void CasinoScene::roulettePos(float out[3]) { std::memcpy(out, RL_POS, sizeof(RL_POS)); }

void CasinoScene::render(const float viewProj[16], const float camPos[3], float time, GLuint liveTex, GLuint liveTop) {
    if (!prog_) return;
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(prog_);
    glUniformMatrix4fv(uViewProj_, 1, GL_FALSE, viewProj);
    glUniform3fv(uCam_, 1, camPos);
    glUniform3fv(uLightPos_, NUM_LIGHTS, LIGHT_POS);
    glUniform3fv(uLightCol_, NUM_LIGHTS, LIGHT_COL);
    glUniform3f(uAmb_, 0.20f, 0.18f, 0.22f);
    glUniform1i(uTexKd_, 0);
    glUniform1i(uTexKe_, 1);

    auto setModel = [&](const Mat& m) { glUniformMatrix4fv(uModel_, 1, GL_FALSE, m.m); };
    // mocheta (4 bucati de 6 x 6 m)
    for (int i = 0; i < 4; ++i) {
        setModel(trans(i % 2 ? 3.0f : -3.0f, 0, i / 2 ? 3.0f : -3.0f));
        floor_.draw(u_);
    }
    // aparatele
    for (int i = 0; i < 3; ++i) {
        setModel(trans(MACHINES_X[i], 0, MACHINES_Z));
        bool live = i == INTERACTIVE;
        slot_.draw(u_, (live && liveTex) ? "ecran" : nullptr, (live && liveTop) ? "ecran_sus" : nullptr);
    }
    // blackjack
    setModel(mul(trans(BJ_POS[0], BJ_POS[1], BJ_POS[2]), rotY(BJ_ROT)));
    blackjack_.draw(u_);
    // ruleta: totul in afara de rotor si bila, apoi rotorul + bila rotite in jurul axei
    Mat rl = mul(trans(RL_POS[0], RL_POS[1], RL_POS[2]), rotY(RL_ROT));
    int rotor = roulette_.findObject("ruleta_rotor"), ball = roulette_.findObject("ruleta_bila");
    for (int o = 0; o < (int)roulette_.objects().size(); ++o) {
        if (o == rotor || o == ball) continue;
        setModel(rl);
        roulette_.drawObject(u_, o);
    }
    Mat spin = mul(trans(WHEEL_CENTER[0], WHEEL_CENTER[1], WHEEL_CENTER[2]),
                   mul(rotY(time * 0.9f), trans(-WHEEL_CENTER[0], -WHEEL_CENTER[1], -WHEEL_CENTER[2])));
    setModel(mul(rl, spin));
    roulette_.drawObject(u_, rotor);
    roulette_.drawObject(u_, ball);
    // ecranele vii (acelasi quad, scalat pentru ecranul de sus)
    for (int k = 0; k < 2; ++k) {
        GLuint t = k == 0 ? liveTex : liveTop;
        if (!t) continue;
        Mat sm;
        std::memcpy(sm.m, k == 0 ? screenLocal_ : topLocal_, sizeof(sm.m));
        if (k == 1) {
            Mat sc = ident();
            sc.m[0] = topW_ / screenW_;
            sc.m[5] = topH_ / screenH_;
            sm = mul(sm, sc);
        }
        setModel(sm);
        float zero[3] = {0, 0, 0}, one[3] = {1, 1, 1}, ks[3] = {0.4f, 0.4f, 0.4f};
        if (u_.kd >= 0) glUniform3fv(u_.kd, 1, zero);
        if (u_.ks >= 0) glUniform3fv(u_.ks, 1, ks);
        if (u_.ns >= 0) glUniform1f(u_.ns, 200.0f);
        if (u_.ke >= 0) glUniform3fv(u_.ke, 1, one);
        if (u_.useTex >= 0) glUniform1i(u_.useTex, 0);
        if (u_.useEmissiveTex >= 0) glUniform1i(u_.useEmissiveTex, 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, t);
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }
    glUseProgram(0);
}

} // namespace cazino
