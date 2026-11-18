#pragma once
// ---------------------------------------------------------------------------
//  SlotMachineUI - interfata unui aparat (lobby + jocuri), desenata intr-o textura.
//
//  Utilizare (vezi README.md pentru detalii):
//      slot::SlotMachineUI::initShared("JocuriPeAparate/assets/");   // o data, dupa glewInit()
//      slot::Wallet portofel;                                        // creditul jucatorului (comun)
//      slot::SlotMachineUI aparat(1280, 720, &portofel);
//      // in bucla principala:
//      aparat.update(dt);
//      aparat.render();                       // deseneaza in textura proprie (FBO)
//      GLuint tex = aparat.texture();         // o pui pe ecranul aparatului din scena
//
//  Intrari: mouse in coordonate de textura (u, v in [0,1], v = 0 jos - exact UV-ul
//  punctului lovit pe ecran) si taste (codurile GLFW, de ex. GLFW_KEY_SPACE).
// ---------------------------------------------------------------------------
#include "SlotGL.hpp"

#include <string>

namespace slot {

// Codurile tastelor = codurile GLFW (poti trimite direct `key` din callback-ul GLFW).
enum Key {
    KEY_SPACE = 32,
    KEY_MINUS = 45,
    KEY_0 = 48, KEY_1 = 49, KEY_2 = 50,
    KEY_EQUAL = 61,
    KEY_A = 65, KEY_B = 66, KEY_C = 67, KEY_D = 68, KEY_E = 69, KEY_G = 71, KEY_I = 73,
    KEY_M = 77, KEY_N = 78, KEY_Q = 81, KEY_R = 82, KEY_S = 83, KEY_V = 86,
    KEY_ESCAPE = 256, KEY_ENTER = 257, KEY_TAB = 258, KEY_BACKSPACE = 259,
    KEY_RIGHT = 262, KEY_LEFT = 263, KEY_DOWN = 264, KEY_UP = 265,
    KEY_PAGE_UP = 266, KEY_PAGE_DOWN = 267,
    KEY_KP_SUBTRACT = 333, KEY_KP_ADD = 334, KEY_KP_ENTER = 335
};

// Creditul jucatorului, in lei. Poate fi comun mai multor aparate (si mesei de blackjack / ruletei).
struct Wallet {
    double balance;
    Wallet(double start = 500.0) : balance(start) {}
};

class SlotMachineUI {
public:
    // Resurse comune (shader, font, simboluri). Apeleaza o singura data, dupa ce OpenGL e initializat.
    static bool initShared(const std::string& assetDir = "JocuriPeAparate/assets/");
    static void shutdownShared();
    static const std::string& lastError();

    // texWidth x texHeight = rezolutia texturii ecranului (recomandat 16:9, ex. 1280x720 sau 1920x1080).
    // wallet = creditul folosit (daca e nullptr, aparatul are propriul credit de 500 lei).
    explicit SlotMachineUI(int texWidth = 1280, int texHeight = 720, Wallet* wallet = nullptr);
    ~SlotMachineUI();

    void update(float dt);        // animatii si logica (secunde)
    void render();                // deseneaza in textura (salveaza si reface starea OpenGL)
    GLuint texture() const;       // textura ecranului (GL_TEXTURE_2D, RGBA8, cu mipmap)

    // Ecranul de sus al aparatului (optional): in lobby jackpot-urile, la jocurile clasice
    // tabelul de plati (la pariul curent), la cele cu speciale o imagine a jocului.
    void renderTop();             // deseneaza in a doua textura (aceeasi rezolutie)
    GLuint topTexture() const;    // 0 pana la primul renderTop()
    int width() const;
    int height() const;

    // ---- intrari mouse, in coordonate de textura (v = 0 jos)
    void mouseMove(float u, float v);
    void mouseDown(float u, float v);
    void mouseUp(float u, float v);
    void mouseLeave();
    void scroll(float dy);
    // ---- tastatura (coduri GLFW). Vezi README pentru lista comenzilor.
    void keyPress(int key);

    // ---- stare
    bool inLobby() const;
    bool isBusy() const;              // rotire, speciala, dublaj sau jackpot in desfasurare
    void goToLobby();                 // inchide jocul curent (colecteaza castigul), daca nu e ocupat
    const char* currentGameName() const;
    bool celebrating() const;         // castig mare / jackpot (poti aprinde luminile aparatului)
    double balance() const;
    void addCredit(double lei);       // "introduci bani" in aparat
    Wallet& wallet();

    // ---- sunet (assets/sounds/*.wav). Tasta V porneste / opreste sunetul tuturor aparatelor.
    void setVolume(float v);              // volumul acestui aparat, 0..1 (ex. dupa distanta pana la camera)
    static void setMasterVolume(float v); // volumul general, 0..1
    static void setMuted(bool m);
    static bool muted();

private:
    SlotMachineUI(const SlotMachineUI&);
    SlotMachineUI& operator=(const SlotMachineUI&);
    struct Impl;
    Impl* p_;
};

} // namespace slot
