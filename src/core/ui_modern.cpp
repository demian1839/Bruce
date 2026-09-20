#if defined(BRUCE_UI_MODERN)

#include "ui_modern.h"
#include "core/wifi/webInterface.h" // isWebUIActive
#include "core/wifi/wg.h"           // isConnectedWireguard
#include "settings.h"               // timeStr, getBattery
#include "utils.h"
#include <MenuItemInterface.h>
#include <interface.h> // isCharging

namespace ui2 {

/* ------------------------------------------------------------------ palette */

uint16_t mix(uint16_t a, uint16_t b, uint8_t t) {
    uint16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    uint16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    uint16_t r = (ar * (255 - t) + br * t) / 255;
    uint16_t g = (ag * (255 - t) + bg * t) / 255;
    uint16_t bl = (ab * (255 - t) + bb * t) / 255;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

uint16_t cLine() { return mix(bruceConfig.bgColor, bruceConfig.priColor, 80); }
uint16_t cSurface() { return mix(bruceConfig.bgColor, bruceConfig.priColor, 24); }
uint16_t cText() { return mix(bruceConfig.priColor, TFT_WHITE, 150); }
uint16_t cMuted() { return mix(bruceConfig.bgColor, bruceConfig.priColor, 150); }

/* -------------------------------------------------------------------- fonts */

void useFont(uint8_t level) {
    switch (level) {
        case 2: tft.setFont(&fonts::FreeSans18pt7b); break;
        case 1: tft.setFont(&fonts::FreeSans12pt7b); break;
        default: tft.setFont(&fonts::FreeSans9pt7b); break;
    }
    tft.setTextSize(1);
}

void resetFont() {
    tft.setFont(&fonts::Font0);
    tft.setTextSize(1);
}

// truncate to the given pixel width, assumes the wanted font is already active
static String fit(const String &s, int maxW) {
    if (tft.textWidth(s) <= maxW) return s;
    String out = s;
    while (out.length() > 1 && tft.textWidth(out + "..") > maxW) out.remove(out.length() - 1);
    return out + "..";
}

static int footerTop() { return tftHeight + 1; }
static int footerHeight() { return tft.height() - tftHeight - 1; }

/* ------------------------------------------------------------------- header */

static void statusChip(int x, int y, void (*icon)(int, int)) {
    tft.fillRoundRect(x, y, 22, 20, 5, bruceConfig.bgColor);
    tft.drawRoundRect(x, y, 22, 20, 5, cLine());
    icon(x + 3, y + 2);
}

static void batteryPill(uint8_t bat) {
    const int w = 40, h = 16;
    int x = tftWidth - 14 - w;
    int y = (HEADER_H - h) / 2;
    uint16_t fg = bat < 16 ? (uint16_t)TFT_RED : bruceConfig.priColor;

    tft.fillRect(x - 60, 0, tftWidth - x + 60, HEADER_H, cSurface());
    tft.drawRoundRect(x, y, w, h, 4, fg);
    tft.fillRect(x + w + 1, y + 5, 3, 6, fg);

    int fill = (w - 6) * bat / 100;
    if (fill > 0) tft.fillRoundRect(x + 3, y + 3, fill, h - 6, 2, fg);

    useFont(0);
    tft.setTextColor(isCharging() ? bruceConfig.priColor : cText(), cSurface());
    tft.drawRightString(isCharging() ? String("CHG") : String(bat) + "%", x - 8, y + 1);
}

static bool s_showBack = false;

void setBackAffordance(bool on) { s_showBack = on; }

bool backHitTest(int16_t x, int16_t y) { return s_showBack && x < 62 && y < HEADER_H + 6; }

void headerBar() {
    uint16_t surf = cSurface();
    tft.fillRect(0, 0, tftWidth, HEADER_H, surf);
    tft.drawFastHLine(0, HEADER_H, tftWidth, cLine());

    int textX = 14;
    if (s_showBack) {
        tft.fillRoundRect(8, 3, 48, 20, 6, bruceConfig.bgColor);
        tft.drawRoundRect(8, 3, 48, 20, 6, cLine());
        useFont(0);
        tft.setTextColor(cText(), bruceConfig.bgColor);
        tft.drawCentreString("BACK", 32, 6);
        textX = 68;
    }

    useFont(0);
    tft.setTextColor(cText(), surf);
    if (clock_set) {
#if defined(HAS_RTC)
        updateTimeStr(_rtc.getTimeStruct());
#else
        updateTimeStr(rtc.getTimeStruct());
#endif
        tft.drawString(timeStr, textX, 4);
    } else {
        tft.drawString("BRUCE", textX, 4);
        tft.setTextColor(cMuted(), surf);
        tft.drawString(BRUCE_VERSION, textX + tft.textWidth("BRUCE ") + 4, 4);
    }

    // status chips, centered
    void (*icons[6])(int, int);
    int n = 0;
    if (sdcardMounted) icons[n++] = drawSdSmall;
    if (gpsConnected) icons[n++] = drawGpsSmall;
    if (WiFi.getMode() != 0) icons[n++] = drawWifiSmall;
    if (isWebUIActive) icons[n++] = drawWebUISmall;
    if (BLEConnected) icons[n++] = drawBLESmall;
    if (isConnectedWireguard) icons[n++] = drawWireguardStatus;

    const int slot = 28;
    int maxW = 6 * slot;
    tft.fillRect((tftWidth - maxW) / 2, 0, maxW, HEADER_H, surf);
    int sx = (tftWidth - (n * slot - 6)) / 2;
    for (int i = 0; i < n; i++) statusChip(sx + i * slot, 3, icons[i]);

    uint8_t bat = getBattery();
    if (bat > 0) batteryPill(bat);

    resetFont();
}

/* ---------------------------------------------------------------- nav footer */

void navBar(uint16_t color, const char *left, const char *center, const char *right) {
    int strip = footerHeight();
    if (strip < 20) return;

    int h = strip > 40 ? 36 : strip - 4;
    int y = footerTop() + (strip - h) / 2;
    int pad = 14, gap = 10;
    int w = (tftWidth - 2 * pad - 2 * gap) / 3;

    tft.fillRect(0, footerTop(), tftWidth, strip, bruceConfig.bgColor);
    useFont(0);

    const char *labels[3] = {left, center, right};
    for (int i = 0; i < 3; i++) {
        int x = pad + i * (w + gap);
        if (i == 1) {
            tft.fillRoundRect(x, y, w, h, 8, color);
            tft.setTextColor(bruceConfig.bgColor, color);
        } else {
            tft.fillRoundRect(x, y, w, h, 8, cSurface());
            tft.drawRoundRect(x, y, w, h, 8, cLine());
            tft.setTextColor(cText(), cSurface());
        }
        tft.drawCentreString(labels[i], x + w / 2, y + (h - 14) / 2);
    }
    resetFont();
}

/* ----------------------------------------------------------------- app grid */

struct Geom {
    int cols, rows, perPage, x0, y0, tw, th, gap;
};

static Geom geom() {
    Geom g;
    g.gap = 10;
    g.cols = tftWidth >= 640 ? 5 : 3;
    g.rows = 3;
    g.perPage = g.cols * g.rows;
    int top = HEADER_H + 10;
    int avail = (tftHeight - 12) - top;
    g.tw = (tftWidth - 32 - (g.cols - 1) * g.gap) / g.cols;
    g.th = (avail - (g.rows - 1) * g.gap) / g.rows;
    g.x0 = (tftWidth - (g.cols * g.tw + (g.cols - 1) * g.gap)) / 2;
    g.y0 = top;
    return g;
}

static int s_gridBase = 0;

int gridHitTest(int16_t x, int16_t y, int count) {
    Geom g = geom();
    for (int slot = 0; slot < g.perPage; slot++) {
        int tx = g.x0 + (slot % g.cols) * (g.tw + g.gap);
        int ty = g.y0 + (slot / g.cols) * (g.th + g.gap);
        if (x >= tx && x < tx + g.tw && y >= ty && y < ty + g.th) {
            int idx = s_gridBase + slot;
            return idx < count ? idx : -1;
        }
    }
    return -1;
}

static void tile(const Geom &g, int slot, Option &opt, bool selected) {
    int x = g.x0 + (slot % g.cols) * (g.tw + g.gap);
    int y = g.y0 + (slot / g.cols) * (g.th + g.gap);

    tft.fillRect(x, y, g.tw, g.th, bruceConfig.bgColor);
    tft.drawRoundRect(x, y, g.tw, g.th, 12, selected ? bruceConfig.priColor : cLine());
    if (selected) {
        tft.drawRoundRect(x + 1, y + 1, g.tw - 2, g.th - 2, 11, bruceConfig.priColor);
        tft.drawRoundRect(
            x + 2, y + 2, g.tw - 4, g.th - 4, 10, mix(bruceConfig.bgColor, bruceConfig.priColor, 110)
        );
    }

    const int labelH = 24;
    int area = min(g.tw - 18, g.th - labelH - 14);
    MenuItemInterface *item = static_cast<MenuItemInterface *>(opt.hoverPointer);
    if (item) item->drawIconAt(x + g.tw / 2, y + (g.th - labelH) / 2 + 2, area, 1.0f);

    useFont(0);
    tft.setTextColor(selected ? bruceConfig.priColor : cText(), bruceConfig.bgColor);
    tft.drawCentreString(fit(opt.label, g.tw - 12), x + g.tw / 2, y + g.th - labelH);
    resetFont();
}

static void pageDots(int page, int pages) {
    int y = tftHeight - 8;
    tft.fillRect(0, y - 3, tftWidth, 8, bruceConfig.bgColor);
    if (pages < 2) return;
    int w = pages * 16 - 8;
    int x = (tftWidth - w) / 2;
    for (int i = 0; i < pages; i++) {
        if (i == page) tft.fillRoundRect(x + i * 16, y - 2, 10, 4, 2, bruceConfig.priColor);
        else tft.fillRoundRect(x + i * 16, y - 1, 8, 2, 1, cLine());
    }
}

bool mainGrid(int index, std::vector<Option> &options, bool firstRender) {
    if (options.empty()) return false;
    // themed icon packs keep the classic single icon layout
    for (auto &o : options) {
        MenuItemInterface *item = static_cast<MenuItemInterface *>(o.hoverPointer);
        if (!item || item->checkTheme()) return false;
    }

    Geom g = geom();
    int count = options.size();
    int page = index / g.perPage;
    int base = page * g.perPage;

    static int lastBase = -1;
    static int lastIndex = -1;
    bool full = firstRender || base != lastBase || lastIndex < 0;

    s_gridBase = base;

    if (full) {
        tft.fillRect(0, HEADER_H + 1, tftWidth, tftHeight - HEADER_H - 1, bruceConfig.bgColor);
        for (int slot = 0; slot < g.perPage && base + slot < count; slot++) {
            tile(g, slot, options[base + slot], base + slot == index);
        }
        pageDots(page, (count + g.perPage - 1) / g.perPage);
    } else {
        if (lastIndex >= base && lastIndex < base + g.perPage && lastIndex < count && lastIndex != index)
            tile(g, lastIndex - base, options[lastIndex], false);
        tile(g, index - base, options[index], true);
    }

    lastBase = base;
    lastIndex = index;

    headerBar();
    if (full) navBar(bruceConfig.priColor, "< PREV", "OPEN", "NEXT >");
    return true;
}

/* -------------------------------------------------------------- option list */

int maxListRows() {
    int rows = (tftHeight - HEADER_H - 60) / 40;
    return rows < 3 ? 3 : rows;
}

struct ListGeom {
    int x, y, w, h, rowH, titleH, rows, init;
};

static ListGeom listGeom(int count, int index) {
    ListGeom l;
    l.rowH = 40;
    l.rows = min(count, maxListRows());
    l.titleH = menuOptionLabel.length() > 0 ? 34 : 12;
    l.w = constrain(tftWidth * 3 / 5, 260, 640);
    l.h = l.titleH + l.rows * l.rowH + 12;
    l.x = (tftWidth - l.w) / 2;
    l.y = HEADER_H + ((tftHeight - HEADER_H) - l.h) / 2;
    if (l.y < HEADER_H + 6) l.y = HEADER_H + 6;
    l.init = index >= l.rows ? index - l.rows + 1 : 0;
    return l;
}

static ListGeom s_listGeom = {0, 0, 0, 0, 40, 12, 0, 0};

int listHitTest(int16_t x, int16_t y, int count) {
    const ListGeom &l = s_listGeom;
    if (l.rows <= 0) return -1;
    if (x < l.x || x > l.x + l.w) return -1;
    int top = l.y + l.titleH + 6;
    if (y < top) return -1;
    int row = (y - top) / l.rowH;
    if (row < 0 || row >= l.rows) return -1;
    int idx = l.init + row;
    return idx < count ? idx : -1;
}

static void listRow(const ListGeom &l, int row, Option &opt, bool selected, uint16_t fg, uint16_t sel) {
    int x = l.x + 6;
    int w = l.w - 12;
    int y = l.y + l.titleH + 6 + row * l.rowH;

    tft.fillRect(x, y, w, l.rowH, bruceConfig.bgColor);

    uint16_t txt = opt.selected ? sel : fg;
    if (!opt.enabled) txt = mix(bruceConfig.bgColor, TFT_WHITE, 90);

    if (selected) {
        tft.fillRoundRect(x, y + 2, w, l.rowH - 4, 8, bruceConfig.priColor);
        txt = bruceConfig.bgColor;
    }

    useFont(1);
    tft.setTextColor(txt, selected ? bruceConfig.priColor : bruceConfig.bgColor);
    tft.drawString(fit(opt.label, w - 46), x + 18, y + (l.rowH - 22) / 2);

    if (opt.selected) {
        int cx = x + w - 26, cy = y + l.rowH / 2;
        uint16_t c = selected ? bruceConfig.bgColor : sel;
        tft.drawWideLine(cx - 6, cy, cx - 2, cy + 5, 3, c, bruceConfig.bgColor);
        tft.drawWideLine(cx - 2, cy + 5, cx + 7, cy - 6, 3, c, bruceConfig.bgColor);
    }
    resetFont();
}

static void listScrollbar(const ListGeom &l, int count, int index) {
    int x = l.x + l.w - 6;
    int top = l.y + l.titleH + 8;
    int h = l.rows * l.rowH - 8;
    if (h <= 0) return;
    if (count <= l.rows) {
        tft.fillRect(x - 1, top, 5, h, bruceConfig.bgColor);
        return;
    }
    tft.fillRect(x - 1, top, 5, h, bruceConfig.bgColor);
    tft.fillRoundRect(x, top, 2, h, 1, cLine());
    int th = max(18, h * l.rows / count);
    int ty = top + (h - th) * index / max(1, count - 1);
    tft.fillRoundRect(x - 1, ty, 4, th, 2, bruceConfig.priColor);
}

Opt_Coord optionList(
    int index, std::vector<Option> &options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor,
    bool firstRender
) {
    Opt_Coord coord;
    int count = options.size();
    ListGeom l = listGeom(count, index);

    static int lastIndex = -1;
    static int lastInit = -1;
    static int lastCount = -1;

    bool full = firstRender || l.init != lastInit || count != lastCount || lastIndex < 0;

    if (firstRender) {
        tft.fillRect(0, HEADER_H + 1, tftWidth, tftHeight - HEADER_H - 1, bruceConfig.bgColor);
        tft.drawRoundRect(l.x, l.y, l.w, l.h, 14, cLine());
        if (l.titleH > 12) {
            String t = menuOptionLabel;
            t.toUpperCase();
            useFont(0);
            tft.setTextColor(cMuted(), bruceConfig.bgColor);
            tft.drawString(fit(t, l.w - 48), l.x + 24, l.y + 10);
            tft.drawFastHLine(l.x + 14, l.y + l.titleH - 4, l.w - 28, cLine());
            resetFont();
        }
    }

    if (full) {
        for (int row = 0; row < l.rows; row++) {
            int idx = l.init + row;
            if (idx >= count) break;
            listRow(l, row, options[idx], idx == index, fgcolor, selcolor);
        }
    } else {
        if (lastIndex >= l.init && lastIndex < l.init + l.rows && lastIndex < count && lastIndex != index)
            listRow(l, lastIndex - l.init, options[lastIndex], false, fgcolor, selcolor);
        listRow(l, index - l.init, options[index], true, fgcolor, selcolor);
    }
    listScrollbar(l, count, index);

    lastIndex = index;
    lastInit = l.init;
    lastCount = count;
    s_listGeom = l;

    // proportional font, so the caller must not scroll the label character by character
    coord.x = l.x + 24;
    coord.y = l.y + l.titleH + 6 + (index - l.init) * l.rowH;
    coord.size = 250;
    coord.fgcolor = fgcolor;
    coord.bgcolor = bgcolor;

    if (firstRender) navBar(bruceConfig.priColor, "< PREV", "OK", "NEXT >");
    return coord;
}

/* ------------------------------------------------------------------ carousel */

void carousel(int index, std::vector<Option> &options, const char *title) {
    int count = options.size();
    if (count == 0) return;
    int top = HEADER_H + 1;
    int area = tftHeight - top;
    int middle = top + area / 2;

    tft.fillRect(0, top, tftWidth, area, bruceConfig.bgColor);

    if (title && strlen(title) > 0) {
        String t = String(title);
        t.toUpperCase();
        useFont(0);
        tft.setTextColor(cMuted(), bruceConfig.bgColor);
        tft.drawString(fit(t, tftWidth - 80), 24, top + 12);
    }

    int prev = index - 1 >= 0 ? index - 1 : count - 1;
    int next = index + 1 < count ? index + 1 : 0;
    int step = area / 4;

    useFont(1);
    tft.setTextColor(options[prev].enabled ? cMuted() : cLine(), bruceConfig.bgColor);
    tft.drawCentreString(fit(options[prev].label, tftWidth - 90), tftWidth / 2, middle - step - 12);
    tft.setTextColor(options[next].enabled ? cMuted() : cLine(), bruceConfig.bgColor);
    tft.drawCentreString(fit(options[next].label, tftWidth - 90), tftWidth / 2, middle + step - 12);

    useFont(2);
    uint16_t c = options[index].enabled ? bruceConfig.priColor : cLine();
    tft.setTextColor(c, bruceConfig.bgColor);
    String label = fit(options[index].label, tftWidth - 110);
    tft.drawCentreString(label, tftWidth / 2, middle - 20);
    int lw = tft.textWidth(label);
    tft.fillRoundRect(tftWidth / 2 - lw / 2, middle + 22, lw, 3, 2, c);
    resetFont();

    // position rail
    int railX = tftWidth - 12;
    int railTop = top + 16;
    int railH = area - 32;
    if (railH > 30) {
        tft.fillRoundRect(railX, railTop, 3, railH, 2, cLine());
        int th = max(22, railH / count);
        int ty = railTop + (railH - th) * index / max(1, count - 1);
        tft.fillRoundRect(railX - 1, ty, 5, th, 3, bruceConfig.priColor);
    }

    headerBar();
}

} // namespace ui2

#endif // BRUCE_UI_MODERN
