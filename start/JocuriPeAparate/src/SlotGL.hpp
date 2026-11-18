#pragma once
// ---------------------------------------------------------------------------
//  Alegerea loader-ului OpenGL.
//  Implicit se foloseste GLEW (ca in laboratoarele de Prelucrare Grafica).
//  Daca folosesti GLAD, defineste SLOTUI_USE_GLAD inainte de a include
//  fisierele modulului (sau in proprietatile proiectului: C/C++ > Preprocessor).
//  Poti indica si un header oarecare:  #define SLOTUI_GL_HEADER <glad/gl.h>
// ---------------------------------------------------------------------------
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX   // evita macro-urile min/max din windows.h
#endif

#if defined(SLOTUI_GL_HEADER)
#include SLOTUI_GL_HEADER
#elif defined(SLOTUI_USE_GLAD)
#include <glad/glad.h>
#else
#include <GL/glew.h>
#endif

// Pentru siruri cu diacritice in codul sursa (fisierele sunt UTF-8 cu BOM).
// Functioneaza la fel in C++14/17/20 si nu depinde de setarea /utf-8 din Visual Studio.
#ifndef SLOT_U8
#define SLOT_U8(s) reinterpret_cast<const char*>(u8##s)
#endif
