#pragma once
// ---------------------------------------------------------------------------
//  Catalogul de jocuri: tab-urile din lobby si lista de jocuri.
//  Ca sa redenumesti un tab sau un joc, ori sa adaugi jocuri noi,
//  editeaza GameCatalog.cpp (functia buildCatalog).
// ---------------------------------------------------------------------------
#include "SlotEngine.hpp"

#include <string>
#include <vector>

namespace slot {

struct Catalog {
    std::vector<std::string> tabNames;   // ordinea tab-urilor din lobby (primul e selectat implicit)
    std::vector<GameDef> games;
};

const Catalog& catalog();
std::vector<int> gamesInTab(int tab);   // indicii jocurilor dintr-un tab, in ordine

// Treptele de pariu total (lei)
extern const double BET_STEPS[];
extern const int BET_STEP_COUNT;
int minBetStep(const GameDef& g);       // prima treapta cu cel putin 0,01 lei pe linie

} // namespace slot
