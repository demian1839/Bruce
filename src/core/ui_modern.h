#ifndef __UI_MODERN_H__
#define __UI_MODERN_H__

#if defined(BRUCE_UI_MODERN)

#include "display.h"
#include <globals.h>

// Alternative front-end for large touch panels (CrowPanel 7.0, 800x480).
// Everything here is opt-in through -DBRUCE_UI_MODERN, the classic drawing
// routines stay untouched for every other target.
namespace ui2 {

static const int HEADER_H = 26;

// palette derived from the user configured priColor / bgColor
uint16_t mix(uint16_t a, uint16_t b, uint8_t t);
uint16_t cLine();    // hairlines, inactive borders
uint16_t cSurface(); // header band, chips
uint16_t cText();    // body text
uint16_t cMuted();   // secondary text

void useFont(uint8_t level); // 0 = small, 1 = body, 2 = display
void resetFont();

int maxListRows();

void setBackAffordance(bool on);
bool backHitTest(int16_t x, int16_t y);

void headerBar();
void navBar(uint16_t color, const char *left, const char *center, const char *right);

bool mainGrid(int index, std::vector<Option> &options, bool firstRender);
Opt_Coord optionList(
    int index, std::vector<Option> &options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor,
    bool firstRender
);
void carousel(int index, std::vector<Option> &options, const char *title);

// -1 when the coordinate does not belong to an item
int gridHitTest(int16_t x, int16_t y, int count);
int listHitTest(int16_t x, int16_t y, int count);

} // namespace ui2

#endif // BRUCE_UI_MODERN
#endif // __UI_MODERN_H__
