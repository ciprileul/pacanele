#pragma once
// ---------------------------------------------------------------------------
//  ObjModel - incarcator simplu de modele OBJ + MTL pentru OpenGL 3.3+.
//
//  Citeste: v / vt / vn / f (triunghiuri sau poligoane), o (obiecte), usemtl,
//  mtllib si din MTL: Kd, Ks, Ns, Ke, d, map_Kd, map_Ke (texturi PNG).
//  Fiecare obiect ("o nume") poate fi desenat separat, cu propria matrice -
//  asa rotesti rotorul ruletei sau ascunzi ecranul aparatului ca sa pui
//  textura interfetei in locul lui.
//
//  Atributele din VAO:  location 0 = pozitie, 1 = normala, 2 = UV
//  (aceleasi ca in laboratoarele de PG). Texturile se citesc cu decodorul
//  PNG din JocuriPeAparate (slot::loadPngFile).
// ---------------------------------------------------------------------------
#include "../../JocuriPeAparate/src/SlotGL.hpp"

#include <string>
#include <vector>

namespace cazino {

struct ObjMaterial {
    std::string name;
    float kd[3] = {0.8f, 0.8f, 0.8f};    // culoare difuza
    float ks[3] = {0.1f, 0.1f, 0.1f};    // culoare speculara
    float ke[3] = {0, 0, 0};             // emisie (lumini, ecrane)
    float ns = 20.0f;                    // stralucire
    float d = 1.0f;                      // opacitate
    GLuint texKd = 0;                    // map_Kd (0 = fara)
    GLuint texKe = 0;                    // map_Ke (0 = fara)
};

struct ObjPart {        // bucata dintr-un obiect, cu un singur material
    int object;
    int material;
    GLint first;
    GLsizei count;
};

struct ObjObject {
    std::string name;
    float bmin[3], bmax[3];               // cutia care il incadreaza (coordonate model)
};

// Locatiile uniformelor pe care draw() le seteaza pentru fiecare material.
// Orice locatie -1 e ignorata, deci poti folosi doar ce are shaderul tau.
struct ObjUniforms {
    GLint kd = -1, ks = -1, ns = -1, ke = -1;
    GLint useTex = -1;          // int: 1 daca materialul are map_Kd
    GLint useEmissiveTex = -1;  // int: 1 daca materialul are map_Ke
    GLint texUnitKd = 0;        // unitatea de textura pentru map_Kd
    GLint texUnitKe = 1;        // unitatea de textura pentru map_Ke

    // Cauta in program uniformele cu numele: uKd, uKs, uNs, uKe, uUseTex, uUseEmissiveTex.
    static ObjUniforms fromProgram(GLuint program);
};

class ObjModel {
public:
    ObjModel() = default;
    ~ObjModel();
    ObjModel(const ObjModel&) = delete;
    ObjModel& operator=(const ObjModel&) = delete;

    // Incarca fisierul .obj (si .mtl-ul + texturile lui). Necesita context OpenGL activ.
    bool load(const std::string& objPath);
    void release();
    const std::string& error() const { return error_; }

    // Deseneaza tot modelul (fara obiectele "skip" si "skip2", daca sunt date).
    void draw(const ObjUniforms& u, const char* skip = nullptr, const char* skip2 = nullptr) const;
    // Deseneaza un singur obiect (dupa index din objects()).
    void drawObject(const ObjUniforms& u, int object) const;

    int findObject(const std::string& name) const;     // -1 daca nu exista
    const std::vector<ObjObject>& objects() const { return objects_; }
    const std::vector<ObjMaterial>& materials() const { return materials_; }
    int triangleCount() const { return vertexCount_ / 3; }
    const float* boundsMin() const { return bmin_; }
    const float* boundsMax() const { return bmax_; }

private:
    void drawPart(const ObjUniforms& u, const ObjPart& p) const;
    bool loadMtl(const std::string& path);
    GLuint loadTexture(const std::string& path);

    std::vector<ObjMaterial> materials_;
    std::vector<ObjObject> objects_;
    std::vector<ObjPart> parts_;
    std::vector<GLuint> ownedTextures_;
    std::vector<std::string> texturePaths_;
    GLuint vao_ = 0, vbo_ = 0;
    int vertexCount_ = 0;
    float bmin_[3] = {0, 0, 0}, bmax_[3] = {0, 0, 0};
    std::string dir_, error_;
};

} // namespace cazino
