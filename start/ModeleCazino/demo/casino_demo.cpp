// ---------------------------------------------------------------------------
//  Demo cazino: sala cu 3 aparate, masa de blackjack si masa de ruleta.
//  Aparatul din mijloc e jucabil (interfata din JocuriPeAparate pe ecranul lui).
//
//    F1            - doar jocul de la aparat, pe toata fereastra (inca o data = inapoi in sala)
//    F11           - fereastra pe tot ecranul monitorului / inapoi in fereastra
//    F3            - schimba camera: aparat / sala / blackjack / ruleta
//    click dreapta + trage   - roteste camera in jurul tintei
//    F2            - +100 lei credit
//    V             - sunet pornit / oprit
//    mouse-ul (click stanga) si restul tastelor merg la aparat, ca in main_demo.cpp
// ---------------------------------------------------------------------------
#include "../../JocuriPeAparate/src/SlotGL.hpp"
#include <GLFW/glfw3.h>

#include "../../JocuriPeAparate/src/SlotUI.hpp"
#include "../src/CasinoScene.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

// ------------------------------------------------------------------ matematica minima
struct V3 {
    float x, y, z;
};
static V3 v3(float x, float y, float z) {
    V3 v = {x, y, z};
    return v;
}
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static V3 norm(V3 a) {
    float l = std::sqrt(dot(a, a));
    return l > 0 ? mul(a, 1.0f / l) : a;
}

struct M4 {
    float m[16];   // coloana-major (ca OpenGL / GLM)
};
static M4 ident() {
    M4 r;
    std::memset(r.m, 0, sizeof(r.m));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
    return r;
}
static M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
static M4 translate(float x, float y, float z) {
    M4 r = ident();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}
static M4 scale(float x, float y, float z) {
    M4 r = ident();
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}
static M4 rotateX(float a) {
    M4 r = ident();
    float c = std::cos(a), s = std::sin(a);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}
static M4 rotateY(float a) {
    M4 r = ident();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}
static M4 perspective(float fovy, float aspect, float n, float f) {
    M4 r;
    std::memset(r.m, 0, sizeof(r.m));
    float t = 1.0f / std::tan(fovy * 0.5f);
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1;
    r.m[14] = 2 * f * n / (n - f);
    return r;
}
static M4 lookAt(V3 eye, V3 at, V3 up) {
    V3 f = norm(sub(at, eye)), s = norm(cross(f, up)), u = cross(s, f);
    M4 r = ident();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}
static V3 xformPoint(const M4& a, V3 p) {
    float w = a.m[3] * p.x + a.m[7] * p.y + a.m[11] * p.z + a.m[15];
    V3 r = v3(a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12], a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
              a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]);
    return w != 0 ? mul(r, 1.0f / w) : r;
}
static V3 xformDir(const M4& a, V3 d) {
    return v3(a.m[0] * d.x + a.m[4] * d.y + a.m[8] * d.z, a.m[1] * d.x + a.m[5] * d.y + a.m[9] * d.z,
              a.m[2] * d.x + a.m[6] * d.y + a.m[10] * d.z);
}
static M4 inverse(const M4& a) {
    const float* m = a.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    M4 r;
    float id = det != 0 ? 1.0f / det : 0.0f;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * id;
    return r;
}

// ------------------------------------------------------------------ camere
struct CamView {
    const char* name;
    V3 target;
    float yaw, pitch, dist, fov;
};
static CamView gViews[4];
static int gView = 0;
static float gYaw = 0, gPitch = 0, gDist = 1;
static bool gDrag = false;
static double gDragX = 0, gDragY = 0;
static slot::SlotMachineUI* gUI = nullptr;
static cazino::CasinoScene* gScene = nullptr;
static M4 gViewProj;
static bool gFlat = false;                 // F1: doar jocul, pe toata fereastra
static int gWinX = 100, gWinY = 100, gWinW = 1280, gWinH = 720;

// dreptunghiul (16:9) in care se deseneaza jocul cand e pe toata fereastra
static void flatRect(int ww, int wh, float& rx, float& ry, float& rw, float& rh) {
    float s = std::fmin(ww / 16.0f, wh / 9.0f);
    rw = 16.0f * s;
    rh = 9.0f * s;
    rx = (ww - rw) * 0.5f;
    ry = (wh - rh) * 0.5f;
}

static void setView(int i) {
    gView = i;
    gYaw = gViews[i].yaw;
    gPitch = gViews[i].pitch;
    gDist = gViews[i].dist;
}

static V3 eyePos() {
    const CamView& v = gViews[gView];
    return add(v.target, v3(gDist * std::cos(gPitch) * std::sin(gYaw), gDist * std::sin(gPitch), gDist * std::cos(gPitch) * std::cos(gYaw)));
}

// mouse (pixeli) -> UV pe ecranul aparatului jucabil, prin raza din camera
static bool mouseToUV(GLFWwindow* win, double mx, double my, float& u, float& v) {
    int ww, wh;
    glfwGetWindowSize(win, &ww, &wh);
    if (ww <= 0 || wh <= 0 || !gScene) return false;
    if (gFlat) {
        float rx, ry, rw, rh;
        flatRect(ww, wh, rx, ry, rw, rh);
        u = (float)((mx - rx) / rw);
        v = 1.0f - (float)((my - ry) / rh);
        return u >= 0 && u <= 1 && v >= 0 && v <= 1;
    }
    float nx = (float)(2.0 * mx / ww - 1.0), ny = (float)(1.0 - 2.0 * my / wh);
    M4 inv = inverse(gViewProj);
    V3 p0 = xformPoint(inv, v3(nx, ny, -1.0f));
    V3 p1 = xformPoint(inv, v3(nx, ny, 1.0f));
    V3 dir = norm(sub(p1, p0));
    M4 sm;
    gScene->screenMatrix(sm.m);
    V3 c = xformPoint(sm, v3(0, 0, 0));
    V3 n = norm(xformDir(sm, v3(0, 0, 1)));
    float den = dot(dir, n);
    if (std::fabs(den) < 1e-6f) return false;
    float t = dot(sub(c, p0), n) / den;
    if (t < 0) return false;
    V3 local = xformPoint(inverse(sm), add(p0, mul(dir, t)));
    u = local.x / gScene->screenWidth() + 0.5f;
    v = local.y / gScene->screenHeight() + 0.5f;
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
}

static void onCursor(GLFWwindow* w, double x, double y) {
    if (gDrag) {
        gYaw -= (float)(x - gDragX) * 0.006f;
        gPitch = std::fmax(-0.1f, std::fmin(1.35f, gPitch + (float)(y - gDragY) * 0.006f));
        gDragX = x;
        gDragY = y;
        return;
    }
    float u, v;
    if (mouseToUV(w, x, y, u, v)) gUI->mouseMove(u, v);
    else gUI->mouseLeave();
}
static void onButton(GLFWwindow* w, int button, int action, int) {
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    if (button == GLFW_MOUSE_BUTTON_RIGHT && !gFlat) {
        gDrag = action == GLFW_PRESS;
        gDragX = x;
        gDragY = y;
        return;
    }
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    float u, v;
    bool on = mouseToUV(w, x, y, u, v);
    if (action == GLFW_PRESS && on) gUI->mouseDown(u, v);
    else if (action == GLFW_RELEASE) gUI->mouseUp(u, v);
}
static void onScroll(GLFWwindow* w, double, double dy) {
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    float u, v;
    if (mouseToUV(w, x, y, u, v)) gUI->scroll((float)dy);          // pe ecran: deruleaza in aparat
    else if (!gFlat) gDist = std::fmax(0.4f, std::fmin(9.0f, gDist * (dy > 0 ? 0.9f : 1.1f)));   // altfel: zoom
}
static void onKey(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        gFlat = !gFlat;
        gDrag = false;
        gUI->mouseLeave();
        return;
    }
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        GLFWmonitor* mon = glfwGetWindowMonitor(w);
        if (mon) {
            glfwSetWindowMonitor(w, nullptr, gWinX, gWinY, gWinW, gWinH, 0);
        } else {
            glfwGetWindowPos(w, &gWinX, &gWinY);
            glfwGetWindowSize(w, &gWinW, &gWinH);
            GLFWmonitor* pm = glfwGetPrimaryMonitor();
            const GLFWvidmode* vm = pm ? glfwGetVideoMode(pm) : nullptr;
            if (pm && vm) glfwSetWindowMonitor(w, pm, 0, 0, vm->width, vm->height, vm->refreshRate);
        }
        return;
    }
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        setView((gView + 1) % 4);
        return;
    }
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        gUI->addCredit(rand() % 500 - 250);
        return;
    }
    if (key == GLFW_KEY_F10) {
        glfwSetWindowShouldClose(w, 1);
        return;
    }
    if (key == GLFW_KEY_F4) exit(0);
    gUI->keyPress(key);
}

static std::string findDir(const char* const* candidates, int n, const char* probe) {
    for (int i = 0; i < n; ++i) {
        std::ifstream f((std::string(candidates[i]) + probe).c_str(), std::ios::binary);
        if (f) return candidates[i];
    }
    return candidates[0];
}

static const char* kFlatVS = R"(#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";
static const char* kFlatFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() { fragColor = vec4(texture(uTex, vUV).rgb, 1.0); }
)";

static GLuint buildFlatProgram() {
    GLuint p = glCreateProgram();
    const char* src[2] = {kFlatVS, kFlatFS};
    GLenum types[2] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER};
    for (int i = 0; i < 2; ++i) {
        GLuint sh = glCreateShader(types[i]);
        glShaderSource(sh, 1, &src[i], nullptr);
        glCompileShader(sh);
        glAttachShader(p, sh);
        glDeleteShader(sh);
    }
    glLinkProgram(p);
    return p;
}

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit a esuat\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Cazino PG - demo (F3 = camera)", nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "Nu pot crea fereastra OpenGL 4.1\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "glewInit a esuat\n");
        return 1;
    }
    glGetError();
    glEnable(GL_MULTISAMPLE);

    const char* assetDirs[] = {"JocuriPeAparate/assets/", "../JocuriPeAparate/assets/", "../../JocuriPeAparate/assets/", "assets/"};
    const char* modelDirs[] = {"ModeleCazino/models/", "../ModeleCazino/models/", "../../ModeleCazino/models/", "models/", "../models/"};
    std::string assets = findDir(assetDirs, 4, "ui_font_sdf.png");
    std::string models = findDir(modelDirs, 5, "aparat.obj");
    if (!slot::SlotMachineUI::initShared(assets))
        std::fprintf(stderr, "Nu pot incarca resursele aparatului din '%s': %s\n", assets.c_str(), slot::SlotMachineUI::lastError().c_str());
    slot::Wallet wallet(500.0);
    {
        slot::SlotMachineUI machine(1280, 720, &wallet);
        cazino::CasinoScene scene;
        if (!scene.init(models)) std::fprintf(stderr, "Nu pot incarca modelele din '%s': %s\n", models.c_str(), scene.error().c_str());
        gUI = &machine;
        gScene = &scene;

        float mp[3], bj[3], rl[3];
        cazino::CasinoScene::machinePos(mp);
        cazino::CasinoScene::blackjackPos(bj);
        cazino::CasinoScene::roulettePos(rl);
        gViews[0] = CamView{"aparat", v3(mp[0], 1.45f, mp[2] + 0.2f), 0.18f, 0.12f, 1.55f, 0.85f};
        gViews[1] = CamView{"sala", v3(0.0f, 0.8f, -0.4f), 0.0f, 0.36f, 4.9f, 0.95f};
        gViews[2] = CamView{"blackjack", v3(bj[0], 0.8f, bj[2]), 0.55f, 0.62f, 2.4f, 0.8f};
        gViews[3] = CamView{"ruleta", v3(rl[0], 0.8f, rl[2]), -0.55f, 0.65f, 2.5f, 0.8f};
        setView(0);
        GLuint flatProg = buildFlatProgram();
        GLuint flatVao = 0, flatVbo = 0;
        {
            const float q[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
            glGenVertexArrays(1, &flatVao);
            glGenBuffers(1, &flatVbo);
            glBindVertexArray(flatVao);
            glBindBuffer(GL_ARRAY_BUFFER, flatVbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glBindVertexArray(0);
        }

        glfwSetCursorPosCallback(win, onCursor);
        glfwSetMouseButtonCallback(win, onButton);
        glfwSetScrollCallback(win, onScroll);
        glfwSetKeyCallback(win, onKey);

        double last = glfwGetTime(), fpsT = 0;
        int frames = 0;
        while (!glfwWindowShouldClose(win)) {
            double now = glfwGetTime();
            float dt = (float)(now - last);
            last = now;
            machine.update(dt);
            machine.render();      // ecranul de joc
            if (!gFlat) machine.renderTop();   // ecranul de sus (plati / imaginea jocului)

            int fw, fh;
            glfwGetFramebufferSize(win, &fw, &fh);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fw, fh);
            glClearColor(0.03f, 0.02f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (gFlat) {
                // doar jocul: textura aparatului pe toata fereastra (16:9, cu benzi negre daca trebuie)
                float rx, ry, rw, rh;
                flatRect(fw, fh, rx, ry, rw, rh);
                glViewport((GLint)rx, (GLint)ry, (GLsizei)rw, (GLsizei)rh);
                glDisable(GL_DEPTH_TEST);
                glUseProgram(flatProg);
                glUniform1i(glGetUniformLocation(flatProg, "uTex"), 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, machine.texture());
                glBindVertexArray(flatVao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
                glUseProgram(0);
                glViewport(0, 0, fw, fh);
            } else {
                float aspect = fh > 0 ? (float)fw / fh : 1.0f;
                V3 eye = eyePos();
                gViewProj = mul(perspective(gViews[gView].fov, aspect, 0.05f, 60.0f), lookAt(eye, gViews[gView].target, v3(0, 1, 0)));
                float cam[3] = {eye.x, eye.y, eye.z};
                scene.render(gViewProj.m, cam, (float)now, machine.texture(), machine.topTexture());
            }
            glfwSwapBuffers(win);
            glfwPollEvents();

            ++frames;
            fpsT += dt;
            if (fpsT >= 1.0) {
                char title[200];
                std::snprintf(title, sizeof(title), "Cazino PG  |  %s  |  %d FPS  |  credit %.2f lei  (F1 = doar jocul, F3 = camera, F11 = ecran complet)",
                              gFlat ? "doar jocul" : gViews[gView].name, frames, wallet.balance);
                glfwSetWindowTitle(win, title);
                frames = 0;
                fpsT = 0;
            }
        }
        glDeleteBuffers(1, &flatVbo);
        glDeleteVertexArrays(1, &flatVao);
        glDeleteProgram(flatProg);
        gUI = nullptr;
        gScene = nullptr;
    }   // scena si aparatul se distrug cat timp contextul OpenGL inca exista
    slot::SlotMachineUI::shutdownShared();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
