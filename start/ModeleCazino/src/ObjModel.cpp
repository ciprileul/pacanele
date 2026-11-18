#include "ObjModel.hpp"
#include "../../JocuriPeAparate/src/PngDecoder.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace cazino {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

std::string dirOf(const std::string& p) {
    size_t k = p.find_last_of("/\\");
    return k == std::string::npos ? std::string() : p.substr(0, k + 1);
}

// "a/b/c", "a//c", "a/b", "a" -> indici (1-based, negativi = relativi la final)
void parseVertexRef(const char* s, int& v, int& t, int& n) {
    v = t = n = 0;
    v = std::atoi(s);
    const char* p = std::strchr(s, '/');
    if (!p) return;
    ++p;
    if (*p != '/') t = std::atoi(p);
    p = std::strchr(p, '/');
    if (!p) return;
    n = std::atoi(p + 1);
}

// citeste n numere separate prin spatii (fara sscanf, care e "nesigur" pentru MSVC /sdl)
void readFloats(const char* s, float* out, int n) {
    for (int i = 0; i < n; ++i) {
        char* end = nullptr;
        out[i] = std::strtof(s, &end);
        if (end == s) {
            for (int k = i; k < n; ++k) out[k] = 0.0f;
            return;
        }
        s = end;
    }
}

int fixIndex(int i, size_t count) {
    if (i > 0) return i - 1;
    if (i < 0) return (int)count + i;
    return -1;
}

} // namespace

ObjUniforms ObjUniforms::fromProgram(GLuint program) {
    ObjUniforms u;
    u.kd = glGetUniformLocation(program, "uKd");
    u.ks = glGetUniformLocation(program, "uKs");
    u.ns = glGetUniformLocation(program, "uNs");
    u.ke = glGetUniformLocation(program, "uKe");
    u.useTex = glGetUniformLocation(program, "uUseTex");
    u.useEmissiveTex = glGetUniformLocation(program, "uUseEmissiveTex");
    return u;
}

ObjModel::~ObjModel() { release(); }

void ObjModel::release() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (!ownedTextures_.empty()) glDeleteTextures((GLsizei)ownedTextures_.size(), ownedTextures_.data());
    vbo_ = vao_ = 0;
    ownedTextures_.clear();
    texturePaths_.clear();
    materials_.clear();
    objects_.clear();
    parts_.clear();
    vertexCount_ = 0;
}

GLuint ObjModel::loadTexture(const std::string& rel) {
    std::string path = dir_ + rel;
    for (size_t i = 0; i < texturePaths_.size(); ++i)
        if (texturePaths_[i] == path) return ownedTextures_[i];
    slot::PngImage img;
    std::string err;
    if (!slot::loadPngFile(path, img, &err)) {
        std::fprintf(stderr, "[ObjModel] nu pot citi textura %s: %s\n", path.c_str(), err.c_str());
        return 0;
    }
    // convertim la RGBA si intoarcem randurile (OpenGL are originea jos)
    std::vector<unsigned char> rgba((size_t)img.width * img.height * 4);
    for (int y = 0; y < img.height; ++y) {
        const unsigned char* src = &img.pixels[(size_t)(img.height - 1 - y) * img.width * img.channels];
        unsigned char* dst = &rgba[(size_t)y * img.width * 4];
        for (int x = 0; x < img.width; ++x, src += img.channels, dst += 4) {
            switch (img.channels) {
                case 1: dst[0] = dst[1] = dst[2] = src[0]; dst[3] = 255; break;
                case 2: dst[0] = dst[1] = dst[2] = src[0]; dst[3] = src[1]; break;
                case 3: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255; break;
                default: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; break;
            }
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    ownedTextures_.push_back(tex);
    texturePaths_.push_back(path);
    return tex;
}

bool ObjModel::loadMtl(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f) {
        error_ = "nu pot deschide " + path;
        return false;
    }
    std::string line;
    ObjMaterial* cur = nullptr;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        if (key == "newmtl") {
            materials_.push_back(ObjMaterial());
            cur = &materials_.back();
            std::getline(ss, cur->name);
            cur->name = trim(cur->name);
        } else if (!cur) {
            continue;
        } else if (key == "Kd") {
            ss >> cur->kd[0] >> cur->kd[1] >> cur->kd[2];
        } else if (key == "Ks") {
            ss >> cur->ks[0] >> cur->ks[1] >> cur->ks[2];
        } else if (key == "Ke") {
            ss >> cur->ke[0] >> cur->ke[1] >> cur->ke[2];
        } else if (key == "Ns") {
            ss >> cur->ns;
        } else if (key == "d") {
            ss >> cur->d;
        } else if (key == "map_Kd" || key == "map_Ke") {
            std::string rest;
            std::getline(ss, rest);
            rest = trim(rest);
            GLuint t = loadTexture(rest);
            if (key == "map_Kd") cur->texKd = t;
            else cur->texKe = t;
        }
    }
    return true;
}

bool ObjModel::load(const std::string& objPath) {
    release();
    error_.clear();
    std::ifstream f(objPath.c_str());
    if (!f) {
        error_ = "nu pot deschide " + objPath;
        return false;
    }
    dir_ = dirOf(objPath);
    std::vector<float> P, N, T;
    // varfurile finale, grupate pe (obiect, material) in ordinea aparitiei
    struct Bucket {
        int object, material;
        std::vector<float> data;
    };
    std::vector<Bucket> buckets;
    std::map<std::pair<int, int>, size_t> bucketOf;
    std::map<std::string, int> matIndex;
    int curObj = -1, curMat = -1;
    auto ensureObject = [&]() {
        if (curObj < 0) {
            objects_.push_back(ObjObject());
            objects_.back().name = "default";
            curObj = 0;
        }
    };
    auto ensureMaterial = [&]() {
        if (curMat < 0) {
            materials_.push_back(ObjMaterial());
            materials_.back().name = "__implicit";
            curMat = (int)materials_.size() - 1;
            matIndex[materials_.back().name] = curMat;
        }
    };
    std::string line;
    std::vector<int> fv, ft, fn;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        const char* s = line.c_str();
        if (s[0] == 'v' && s[1] == ' ') {
            float a[3];
            readFloats(s + 2, a, 3);
            P.push_back(a[0]); P.push_back(a[1]); P.push_back(a[2]);
        } else if (s[0] == 'v' && s[1] == 't') {
            float a[2];
            readFloats(s + 3, a, 2);
            T.push_back(a[0]); T.push_back(a[1]);
        } else if (s[0] == 'v' && s[1] == 'n') {
            float a[3];
            readFloats(s + 3, a, 3);
            N.push_back(a[0]); N.push_back(a[1]); N.push_back(a[2]);
        } else if (s[0] == 'f' && s[1] == ' ') {
            ensureObject();
            ensureMaterial();
            fv.clear(); ft.clear(); fn.clear();
            std::istringstream ss(line.substr(2));
            std::string tok;
            while (ss >> tok) {
                int v, t, n;
                parseVertexRef(tok.c_str(), v, t, n);
                fv.push_back(fixIndex(v, P.size() / 3));
                ft.push_back(fixIndex(t, T.size() / 2));
                fn.push_back(fixIndex(n, N.size() / 3));
            }
            if (fv.size() < 3) continue;
            std::pair<int, int> key(curObj, curMat);
            if (!bucketOf.count(key)) {
                bucketOf[key] = buckets.size();
                buckets.push_back(Bucket{curObj, curMat, {}});
            }
            std::vector<float>& D = buckets[bucketOf[key]].data;
            // normala fetei, pentru varfurile fara vn
            float fnrm[3] = {0, 1, 0};
            if (fv[0] >= 0 && fv[1] >= 0 && fv[2] >= 0) {
                const float* a = &P[(size_t)fv[0] * 3];
                const float* b = &P[(size_t)fv[1] * 3];
                const float* c = &P[(size_t)fv[2] * 3];
                float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                fnrm[0] = u[1] * w[2] - u[2] * w[1];
                fnrm[1] = u[2] * w[0] - u[0] * w[2];
                fnrm[2] = u[0] * w[1] - u[1] * w[0];
            }
            for (size_t k = 1; k + 1 < fv.size(); ++k) {     // evantai de triunghiuri
                size_t idx[3] = {0, k, k + 1};
                for (int q = 0; q < 3; ++q) {
                    int vi = fv[idx[q]], ti = ft[idx[q]], ni = fn[idx[q]];
                    for (int c = 0; c < 3; ++c) D.push_back(vi >= 0 && (size_t)vi * 3 + c < P.size() ? P[(size_t)vi * 3 + c] : 0.0f);
                    for (int c = 0; c < 3; ++c) D.push_back(ni >= 0 && (size_t)ni * 3 + c < N.size() ? N[(size_t)ni * 3 + c] : fnrm[c]);
                    for (int c = 0; c < 2; ++c) D.push_back(ti >= 0 && (size_t)ti * 2 + c < T.size() ? T[(size_t)ti * 2 + c] : 0.0f);
                }
            }
        } else if (s[0] == 'o' && s[1] == ' ') {
            std::string name = trim(line.substr(2));
            curObj = -1;
            for (size_t i = 0; i < objects_.size(); ++i)
                if (objects_[i].name == name) curObj = (int)i;
            if (curObj < 0) {
                objects_.push_back(ObjObject());
                objects_.back().name = name;
                curObj = (int)objects_.size() - 1;
            }
        } else if (std::strncmp(s, "usemtl", 6) == 0) {
            std::string name = trim(line.substr(6));
            if (!matIndex.count(name)) {
                for (size_t i = 0; i < materials_.size(); ++i)
                    if (materials_[i].name == name) matIndex[name] = (int)i;
            }
            if (!matIndex.count(name)) {
                materials_.push_back(ObjMaterial());
                materials_.back().name = name;
                matIndex[name] = (int)materials_.size() - 1;
            }
            curMat = matIndex[name];
        } else if (std::strncmp(s, "mtllib", 6) == 0) {
            size_t before = materials_.size();
            loadMtl(dir_ + trim(line.substr(6)));
            for (size_t i = before; i < materials_.size(); ++i) matIndex[materials_[i].name] = (int)i;
        }
    }
    // un singur VBO, bucatile una dupa alta
    std::vector<float> all;
    for (size_t b = 0; b < buckets.size(); ++b) {
        ObjPart part;
        part.object = buckets[b].object;
        part.material = buckets[b].material;
        part.first = (GLint)(all.size() / 8);
        part.count = (GLsizei)(buckets[b].data.size() / 8);
        all.insert(all.end(), buckets[b].data.begin(), buckets[b].data.end());
        parts_.push_back(part);
    }
    vertexCount_ = (int)(all.size() / 8);
    if (vertexCount_ == 0) {
        error_ = "modelul nu are fete: " + objPath;
        return false;
    }
    // cutiile de incadrare
    for (size_t o = 0; o < objects_.size(); ++o)
        for (int c = 0; c < 3; ++c) {
            objects_[o].bmin[c] = 1e30f;
            objects_[o].bmax[c] = -1e30f;
        }
    for (int c = 0; c < 3; ++c) {
        bmin_[c] = 1e30f;
        bmax_[c] = -1e30f;
    }
    for (size_t p = 0; p < parts_.size(); ++p) {
        ObjObject& ob = objects_[(size_t)parts_[p].object];
        for (GLsizei i = 0; i < parts_[p].count; ++i) {
            const float* v = &all[(size_t)(parts_[p].first + i) * 8];
            for (int c = 0; c < 3; ++c) {
                ob.bmin[c] = std::min(ob.bmin[c], v[c]);
                ob.bmax[c] = std::max(ob.bmax[c], v[c]);
                bmin_[c] = std::min(bmin_[c], v[c]);
                bmax_[c] = std::max(bmax_[c], v[c]);
            }
        }
    }
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(all.size() * sizeof(float)), all.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

int ObjModel::findObject(const std::string& name) const {
    for (size_t i = 0; i < objects_.size(); ++i)
        if (objects_[i].name == name) return (int)i;
    return -1;
}

void ObjModel::drawPart(const ObjUniforms& u, const ObjPart& p) const {
    const ObjMaterial& m = materials_[(size_t)p.material];
    if (u.kd >= 0) glUniform3fv(u.kd, 1, m.kd);
    if (u.ks >= 0) glUniform3fv(u.ks, 1, m.ks);
    if (u.ke >= 0) glUniform3fv(u.ke, 1, m.ke);
    if (u.ns >= 0) glUniform1f(u.ns, m.ns);
    if (u.useTex >= 0) glUniform1i(u.useTex, m.texKd ? 1 : 0);
    if (u.useEmissiveTex >= 0) glUniform1i(u.useEmissiveTex, m.texKe ? 1 : 0);
    if (m.texKd) {
        glActiveTexture(GL_TEXTURE0 + u.texUnitKd);
        glBindTexture(GL_TEXTURE_2D, m.texKd);
    }
    if (m.texKe) {
        glActiveTexture(GL_TEXTURE0 + u.texUnitKe);
        glBindTexture(GL_TEXTURE_2D, m.texKe);
    }
    glDrawArrays(GL_TRIANGLES, p.first, p.count);
}

void ObjModel::draw(const ObjUniforms& u, const char* skip, const char* skip2) const {
    if (!vao_) return;
    int skipObj = skip ? findObject(skip) : -1;
    int skipObj2 = skip2 ? findObject(skip2) : -1;
    glBindVertexArray(vao_);
    for (size_t i = 0; i < parts_.size(); ++i)
        if (parts_[i].object != skipObj && parts_[i].object != skipObj2) drawPart(u, parts_[i]);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

void ObjModel::drawObject(const ObjUniforms& u, int object) const {
    if (!vao_ || object < 0) return;
    glBindVertexArray(vao_);
    for (size_t i = 0; i < parts_.size(); ++i)
        if (parts_[i].object == object) drawPart(u, parts_[i]);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

} // namespace cazino
