#pragma once
// Decodor PNG minimal (fara dependinte): suporta PNG neintretesut pe 8/16 biti,
// tipurile gri, gri+alfa, RGB, RGBA si paleta. Folosit pentru atlasele din assets/.
#include <cstddef>
#include <string>
#include <vector>

namespace slot {

struct PngImage {
    int width = 0;
    int height = 0;
    int channels = 0;                    // 1 = gri, 2 = gri+alfa, 3 = RGB, 4 = RGBA
    std::vector<unsigned char> pixels;   // randuri de sus in jos
};

bool decodePng(const unsigned char* data, size_t size, PngImage& out, std::string* error = nullptr);
bool loadPngFile(const std::string& path, PngImage& out, std::string* error = nullptr);

} // namespace slot
