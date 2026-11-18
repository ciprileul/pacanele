// ---------------------------------------------------------------------------
//  Demo pentru interfata aparatelor (JocuriPeAparate).
//  Ruleaza fara scena ta: deschide o fereastra GLFW si arata interfata.
//
//    F1  - comuta intre "ecran" (interfata pe toata fereastra) si "3D"
//          (ecranul unui aparat inclinat in spatiu, cu selectie prin raza -
//           exact ce vei face in scena ta)
//    F2  - adauga 100 lei credit
//    V   - porneste / opreste sunetul
//    restul tastelor si mouse-ul merg direct la aparat (vezi README.md)
//
//  Dependinte: GLFW + GLEW (aceleasi ca in laboratoarele de PG). Fara GLM,
//  ca sa poata fi compilat oriunde; in README e aceeasi logica scrisa cu GLM.
// ---------------------------------------------------------------------------
#include "../src/SlotGL.hpp"
#include <GLFW/glfw3.h>

#include "../src/SlotUI.hpp"

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

// ------------------------------------------------------------------ shadere demo
static const char* kVS = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vN;
out vec2 vUV;
void main() {
    vN = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";
static const char* kFS = R"(#version 330 core
in vec3 vN;
in vec2 vUV;
uniform sampler2D uTex;
uniform int uUseTex;
uniform vec3 uColor;
out vec4 fragColor;
void main() {
    if (uUseTex == 1) {
        // ecranul aparatului emite lumina: fara iluminare, doar textura
        fragColor = vec4(texture(uTex, vUV).rgb, 1.0);
        return;
    }
    vec3 L = normalize(vec3(0.4, 0.8, 0.6));
    float d = max(dot(normalize(vN), L), 0.0);
    fragColor = vec4(uColor * (0.25 + 0.75 * d), 1.0);
}
)";

static GLuint compile(GLenum t, const char* s) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader: %s\n", log);
    }
    return sh;
}

// ------------------------------------------------------------------ stare demo
static slot::SlotMachineUI* gUI = nullptr;
static bool g3D = false;
static M4 gViewProj, gScreenModel;
static const float SCREEN_W = 1.6f, SCREEN_H = 0.9f;   // dimensiunea ecranului in scena (16:9)

// Transforma pozitia mouse-ului (pixeli fereastra) in UV pe ecranul aparatului.
// Intoarce false daca mouse-ul nu e pe ecran.
static bool mouseToUV(GLFWwindow* win, double mx, double my, float& u, float& v) {
    int ww, wh;
    glfwGetWindowSize(win, &ww, &wh);
    if (ww <= 0 || wh <= 0) return false;
    if (!g3D) {
        // interfata pe toata fereastra (pastrand 16:9)
        float s = std::fmin(ww / 16.0f, wh / 9.0f);
        float rw = 16.0f * s, rh = 9.0f * s, rx = (ww - rw) * 0.5f, ry = (wh - rh) * 0.5f;
        u = (float)((mx - rx) / rw);
        v = 1.0f - (float)((my - ry) / rh);
    } else {
        // raza din camera prin pixel -> intersectie cu planul ecranului -> UV
        float nx = (float)(2.0 * mx / ww - 1.0), ny = (float)(1.0 - 2.0 * my / wh);
        M4 inv = inverse(gViewProj);
        V3 p0 = xformPoint(inv, v3(nx, ny, -1.0f));
        V3 p1 = xformPoint(inv, v3(nx, ny, 1.0f));
        V3 dir = norm(sub(p1, p0));
        V3 c = xformPoint(gScreenModel, v3(0, 0, 0));
        V3 n = norm(xformDir(gScreenModel, v3(0, 0, 1)));
        float den = dot(dir, n);
        if (std::fabs(den) < 1e-6f) return false;
        float t = dot(sub(c, p0), n) / den;
        if (t < 0) return false;
        V3 hit = add(p0, mul(dir, t));
        V3 local = xformPoint(inverse(gScreenModel), hit);
        u = local.x / SCREEN_W + 0.5f;
        v = local.y / SCREEN_H + 0.5f;
    }
    return u >= 0 && u <= 1 && v >= 0 && v <= 1;
}

static void onCursor(GLFWwindow* w, double x, double y) {
    float u, v;
    if (mouseToUV(w, x, y, u, v)) gUI->mouseMove(u, v);
    else gUI->mouseLeave();
}
static void onButton(GLFWwindow* w, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    float u, v;
    bool on = mouseToUV(w, x, y, u, v);
    if (action == GLFW_PRESS && on) gUI->mouseDown(u, v);
    else if (action == GLFW_RELEASE) gUI->mouseUp(u, v);
}
static void onScroll(GLFWwindow*, double, double dy) {
    gUI->scroll((float)dy);
}
static void onKey(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        g3D = !g3D;
        gUI->mouseLeave();
        return;
    }
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        gUI->addCredit(100);
        return;
    }
    if (key == GLFW_KEY_F10) {
        glfwSetWindowShouldClose(w, 1);
        return;
    }
    gUI->keyPress(key);   // codurile GLFW sunt folosite direct de aparat
}

static std::string findAssets() {
    const char* candidates[] = {"JocuriPeAparate/assets/", "assets/", "../assets/", "../../assets/", "../JocuriPeAparate/assets/"};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        std::ifstream f((std::string(candidates[i]) + "ui_font_sdf.png").c_str(), std::ios::binary);
        if (f) return candidates[i];
    }
    return "JocuriPeAparate/assets/";
}

// cub unitate (pozitie, normala, uv) pentru dulapul aparatului + quad pentru ecran
static void buildMeshes(GLuint& cubeVao, GLuint& quadVao) {
    float cube[36 * 8];
    const float faces[6][4][3] = {
        {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
        {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}},
        {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}},
        {{0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
        {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}},
        {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}}};
    const float normals[6][3] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    int k = 0;
    for (int f = 0; f < 6; ++f)
        for (int i = 0; i < 6; ++i) {
            const float* p = faces[f][idx[i]];
            cube[k++] = p[0]; cube[k++] = p[1]; cube[k++] = p[2];
            cube[k++] = normals[f][0]; cube[k++] = normals[f][1]; cube[k++] = normals[f][2];
            cube[k++] = 0; cube[k++] = 0;
        }
    // ecran: centrat in origine, SCREEN_W x SCREEN_H, orientat spre +Z, UV standard (v = 0 jos)
    const float hw = SCREEN_W * 0.5f, hh = SCREEN_H * 0.5f;
    float quad[6 * 8] = {-hw, -hh, 0, 0, 0, 1, 0, 0, hw, -hh, 0, 0, 0, 1, 1, 0, hw, hh, 0, 0, 0, 1, 1, 1,
                         -hw, -hh, 0, 0, 0, 1, 0, 0, hw, hh, 0, 0, 0, 1, 1, 1, -hw, hh, 0, 0, 0, 1, 0, 1};
    GLuint vbo[2];
    glGenVertexArrays(1, &cubeVao);
    glGenVertexArrays(1, &quadVao);
    glGenBuffers(2, vbo);
    GLuint vaos[2] = {cubeVao, quadVao};
    const float* data[2] = {cube, quad};
    GLsizeiptr sizes[2] = {(GLsizeiptr)sizeof(cube), (GLsizeiptr)sizeof(quad)};
    for (int i = 0; i < 2; ++i) {
        glBindVertexArray(vaos[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
        glBufferData(GL_ARRAY_BUFFER, sizes[i], data[i], GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    }
    glBindVertexArray(0);
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
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Aparat - demo (F1 = ecran / 3D, F2 = +100 lei)", nullptr, nullptr);
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
    glGetError();   // GLEW poate lasa un GL_INVALID_ENUM in profilul core

    std::string assets = findAssets();
    if (!slot::SlotMachineUI::initShared(assets)) {
        std::fprintf(stderr, "Nu pot incarca resursele din '%s': %s\n", assets.c_str(), slot::SlotMachineUI::lastError().c_str());
    }
    slot::Wallet wallet(500.0);   // creditul jucatorului (il poti folosi si la blackjack / ruleta)
    {
    slot::SlotMachineUI machine(1280, 720, &wallet);
    gUI = &machine;

    glfwSetCursorPosCallback(win, onCursor);
    glfwSetMouseButtonCallback(win, onButton);
    glfwSetScrollCallback(win, onScroll);
    glfwSetKeyCallback(win, onKey);

    GLuint prog = glCreateProgram();
    GLuint vs = compile(GL_VERTEX_SHADER, kVS), fs = compile(GL_FRAGMENT_SHADER, kFS);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint uMVP = glGetUniformLocation(prog, "uMVP"), uModel = glGetUniformLocation(prog, "uModel");
    GLint uTex = glGetUniformLocation(prog, "uTex"), uUseTex = glGetUniformLocation(prog, "uUseTex");
    GLint uColor = glGetUniformLocation(prog, "uColor");
    GLuint cubeVao = 0, quadVao = 0;
    buildMeshes(cubeVao, quadVao);

    double last = glfwGetTime(), fpsT = 0;
    int frames = 0;
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)(now - last);
        last = now;

        machine.update(dt);
        machine.render();   // interfata se deseneaza in textura aparatului

        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fw, fh);
        glClearColor(0.04f, 0.03f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(prog);
        glUniform1i(uTex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, machine.texture());

        if (!g3D) {
            // textura pe toata fereastra, 16:9
            glDisable(GL_DEPTH_TEST);
            float aspect = fh > 0 ? (float)fw / fh : 1.0f;
            float sx = 1, sy = 1;
            if (aspect > 16.0f / 9.0f) sx = (16.0f / 9.0f) / aspect;
            else sy = aspect / (16.0f / 9.0f);
            M4 mvp = scale(sx * 2.0f / SCREEN_W, sy * 2.0f / SCREEN_H, 1);
            M4 id = ident();
            glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
            glUniformMatrix4fv(uModel, 1, GL_FALSE, id.m);
            glUniform1i(uUseTex, 1);
            glBindVertexArray(quadVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        } else {
            // aparat simplu in 3D; ecranul e inclinat spre jucator
            glEnable(GL_DEPTH_TEST);
            float aspect = fh > 0 ? (float)fw / fh : 1.0f;
            float orbit = 0.35f * std::sin((float)now * 0.25f);
            V3 eye = v3(2.2f * std::sin(orbit), 1.45f, 2.2f * std::cos(orbit));
            gViewProj = mul(perspective(0.8f, aspect, 0.05f, 50.0f), lookAt(eye, v3(0, 1.3f, 0), v3(0, 1, 0)));
            gScreenModel = mul(translate(0, 1.36f, 0.30f), rotateX(-0.22f));   // ecranul, inclinat pe spate
            struct Box {
                float x, y, z, sx, sy, sz, r, g, b;
            };
            const Box boxes[] = {
                {0, 0.40f, 0, 1.9f, 0.8f, 0.7f, 0.35f, 0.05f, 0.08f},       // baza
                {0, 1.36f, 0.0f, 1.9f, 1.12f, 0.30f, 0.18f, 0.03f, 0.05f},  // corpul din jurul ecranului
                {0, 2.15f, 0.0f, 1.9f, 0.45f, 0.45f, 0.85f, 0.65f, 0.15f},  // panou superior
                {0, 0.80f, 0.40f, 1.7f, 0.06f, 0.30f, 0.1f, 0.1f, 0.12f},   // pupitrul cu butoane
            };
            glUniform1i(uUseTex, 0);
            glBindVertexArray(cubeVao);
            for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); ++i) {
                const Box& b = boxes[i];
                M4 model = mul(translate(b.x, b.y, b.z), scale(b.sx, b.sy, b.sz));
                M4 mvp = mul(gViewProj, model);
                glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
                glUniformMatrix4fv(uModel, 1, GL_FALSE, model.m);
                glUniform3f(uColor, b.r, b.g, b.b);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            M4 mvp = mul(gViewProj, gScreenModel);
            glUniformMatrix4fv(uMVP, 1, GL_FALSE, mvp.m);
            glUniformMatrix4fv(uModel, 1, GL_FALSE, gScreenModel.m);
            glUniform1i(uUseTex, 1);
            glBindVertexArray(quadVao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            (void)rotateY;
        }
        glBindVertexArray(0);
        glfwSwapBuffers(win);
        glfwPollEvents();

        ++frames;
        fpsT += dt;
        if (fpsT >= 1.0) {
            char title[160];
            std::snprintf(title, sizeof(title), "Aparat - demo  |  %s  |  %d FPS  |  credit %.2f lei  (F1 = ecran/3D, F2 = +100 lei, V = sunet)",
                          g3D ? "3D" : "ecran", frames, wallet.balance);
            glfwSetWindowTitle(win, title);
            frames = 0;
            fpsT = 0;
        }
    }
    gUI = nullptr;
    }   // aparatul (FBO-ul lui) se distruge cat timp contextul OpenGL inca exista
    slot::SlotMachineUI::shutdownShared();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
