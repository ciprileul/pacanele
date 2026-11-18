#pragma once
// ---------------------------------------------------------------------------
//  CasinoScene - exemplu complet de scena cu modelele din models/:
//  3 aparate, masa de blackjack, masa de ruleta (cu rotorul care se invarte),
//  mocheta si 5 lumini, cu un shader Blinn-Phong + emisie.
//
//  Il poti folosi direct sau doar ca exemplu pentru scena ta.
//  Ecranul aparatului din mijloc poate primi textura interfetei
//  (SlotMachineUI::texture()) - vezi render(..., liveScreenTex).
// ---------------------------------------------------------------------------
#include "ObjModel.hpp"

namespace cazino {

class CasinoScene {
public:
    bool init(const std::string& modelsDir);     // ex: "ModeleCazino/models/"
    void release();
    const std::string& error() const { return error_; }

    // viewProj: matrice 4x4 (coloane, ca in OpenGL/GLM); camPos: pozitia camerei
    // liveScreenTex: textura de pus pe ecranul de joc al aparatului din mijloc (0 = poza din model)
    // liveTopTex:    textura pentru ecranul lui de sus (SlotMachineUI::topTexture(), 0 = poza din model)
    void render(const float viewProj[16], const float camPos[3], float time, GLuint liveScreenTex, GLuint liveTopTex = 0);

    // Matricea ecranului aparatului interactiv: origine in centrul ecranului,
    // X spre dreapta, Y in sus pe ecran, Z spre jucator. Ecranul are
    // screenWidth() x screenHeight() metri. Folosit pentru selectia cu raza (mouse -> UV).
    void screenMatrix(float out[16]) const;
    float screenWidth() const { return screenW_; }
    float screenHeight() const { return screenH_; }

    // unde se afla lucrurile (ca sa pui camera)
    static void machinePos(float out[3]);
    static void blackjackPos(float out[3]);
    static void roulettePos(float out[3]);

private:
    ObjModel slot_, blackjack_, roulette_, floor_;
    GLuint prog_ = 0, quadVao_ = 0, quadVbo_ = 0;
    ObjUniforms u_;
    GLint uModel_ = -1, uViewProj_ = -1, uCam_ = -1, uLightPos_ = -1, uLightCol_ = -1, uAmb_ = -1;
    GLint uTexKd_ = -1, uTexKe_ = -1;
    float screenW_ = 0.6f, screenH_ = 0.3375f;
    float screenLocal_[16], topLocal_[16];
    float topW_ = 0.6f, topH_ = 0.3375f;
    std::string error_;
};

} // namespace cazino
