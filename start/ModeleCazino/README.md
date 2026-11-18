# ModeleCazino – modele 3D pentru sala de cazino

Modelele sunt originale și generate procedural de `tools/gen_models.py`. Sunt în format **OBJ + MTL + texturi PNG**, deci le poți încărca cu orice loader de OBJ, inclusiv cel din laborator.

| Fișier | Ce conține | Triunghiuri |
|---|---|---|
| `models/aparat.obj` | aparat de păcănele cu două ecrane: soclu, dulap roșu, burtă luminată, pupitru cu 7 butoane etichetate, curbură neagră lucioasă de la butoane spre ecranul de joc, ecranul de joc și ecranul de sus (16:9, ușor înclinate), LED-uri pe contur, firmă și lampă | ~2.000 |
| `models/blackjack.obj` | masă semicirculară cu 7 locuri: postav tipărit, pernă de piele, lemn, tavă cu jetoane, sabot, cărți, 7 scaune | ~27.000 |
| `models/ruleta.obj` | masă de ruletă europeană: postav cu toate pariurile, roată cu 37 de numere în ordinea reală, rotor separat, bilă separată, 3 scaune | ~13.000 |
| `models/podea.obj` | o bucată de mochetă de 6 × 6 m (se repetă) | 2 |

**Convenții:** unitățile sunt metri, axa Y e în sus, iar fața obiectelor e spre **+Z** (acolo stă jucătorul). Originea e pe podea, în centrul obiectului.

## Obiectele din fișiere (`o nume`)

- **aparat.obj**: `aparat_dulap`, `aparat_butoane`, `aparat_firma`, `ecran` și `ecran_sus`.
  - `ecran` (cel de joc) și `ecran_sus` sunt dreptunghiuri de 0,60 × 0,3375 m, cu UV-uri 0..1 (v = 0 jos).
  - Pe `ecran` pui `SlotMachineUI::texture()`, iar pe `ecran_sus` pui `SlotMachineUI::topTexture()`, după ce apelezi `renderTop()`.
  - În scenă îl înlocuiești cu textura interfeței, `SlotMachineUI::texture()`.
- **ruleta.obj**: `ruleta_masa`, `ruleta_roata`, `ruleta_rotor`, `ruleta_bila`, `ruleta_jetoane` și `rl_scaun_1..3`.
  - Rotorul și bila se rotesc în jurul axei verticale care trece prin **(-0,86; 0,82; 0)**, în coordonatele modelului.
- **blackjack.obj**: `bj_masa`, `bj_jetoane`, `bj_sabot` și `bj_scaun_1..7`.

## Demo (Visual Studio, proiectul `start`)

Proiectul `start` compilează acum `ModeleCazino/demo/casino_demo.cpp`. Demo-ul vechi, `main_demo.cpp`, a rămas în proiect, dar e exclus de la compilare. Deschide `start.slnx`, alege **x64** și apasă **F5**.

Sala are 3 aparate, masa de blackjack și masa de ruletă, iar rotorul ruletei se învârte. Aparatul din mijloc e jucabil cu mouse-ul direct pe ecranul lui 3D.

| Tastă / mouse | Ce face |
|---|---|
| **F1** | doar jocul de la aparat, pe toată fereastra (încă o dată = înapoi în sală) |
| **F11** | fereastra pe tot ecranul monitorului / înapoi în fereastră |
| **F3** | schimbă camera: aparat → sală → blackjack → ruletă |
| click dreapta + trage | rotește camera în jurul țintei |
| rotița (în afara ecranului) | zoom |
| click stânga pe ecran | joci la aparat |
| F2 | +100 lei |
| V | sunet pornit / oprit |
| F10 | ieșire |

Ca să revii la demo-ul vechi: click dreapta pe `casino_demo.cpp` → Properties → *Excluded From Build = Yes*, apoi la `main_demo.cpp` pui *No*.

## Cum le folosești în scena ta

```cpp
#include "ModeleCazino/src/ObjModel.hpp"

cazino::ObjModel aparat;
aparat.load("ModeleCazino/models/aparat.obj");        // dupa ce ai contextul OpenGL
cazino::ObjUniforms u = cazino::ObjUniforms::fromProgram(shaderulTau);
// in bucla:
glUseProgram(shaderulTau);
glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(modelAparat));
aparat.draw(u, "ecran", "ecran_sus");   // tot aparatul, fara ecrane (pui tu texturile interfetei pe ele)
```

`draw()` setează pentru fiecare material uniformele `uKd`, `uKs`, `uNs`, `uKe`, `uUseTex` și `uUseEmissiveTex`, dar numai pe cele care există în shaderul tău. Mai leagă `map_Kd` pe unitatea de textură 0 și `map_Ke` pe unitatea 1. Atributele sunt `location 0` = poziție, `1` = normală și `2` = UV.

**Rotorul ruletei** se desenează separat, cu matricea lui:

```cpp
int rotor = ruleta.findObject("ruleta_rotor"), bila = ruleta.findObject("ruleta_bila");
glm::vec3 axa(-0.86f, 0.82f, 0.0f);
glm::mat4 spin = glm::translate(glm::mat4(1), axa) * glm::rotate(glm::mat4(1), unghi, glm::vec3(0,1,0)) * glm::translate(glm::mat4(1), -axa);
// model = modelRuleta * spin;  ruleta.drawObject(u, rotor); ruleta.drawObject(u, bila);
```

**Ecranul de joc** are centrul în (0; 1,398; 0,214) și e înclinat pe spate cu 0,126 rad (≈7,2°). **Ecranul de sus** are centrul în (0; 1,795; 0,171) și e înclinat cu 0,087 rad (5°). `CasinoScene::screenMatrix()` îți dă matricea ecranului de joc, iar `casino_demo.cpp` arată cum transformi mouse-ul în UV.

`src/CasinoScene.*` e o scenă completă: shader Blinn-Phong cu 5 lumini și emisie (ecrane, LED-uri, firme), plus așezarea obiectelor. O poți folosi direct sau doar ca exemplu.

## Regenerare

```
python3 tools/gen_models.py
```

Ai nevoie de Python 3 cu `numpy` și `Pillow`. Măsurile, culorile și texturile se schimbă din `tools/gen_models.py` și `tools/textures.py`. `tools/ecran_aparat.png` e poza pusă pe ecranul aparatului când nu folosești interfața vie.
