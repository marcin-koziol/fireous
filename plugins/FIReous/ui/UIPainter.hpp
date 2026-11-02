/*
 * FIReous - hardware-DSP style Cairo layout and painting.
 *
 * Layout: 1280x900 fixed logical size (scaled by UiScale). Five modules are
 * placed in a 3x2 rack: the first chain position gets a double-width slot,
 * the other four get normal slots. This keeps the wide Graphic EQ from the
 * mockup while still allowing any module to be dragged into any position.
 */

#pragma once

#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#include "../Params.hpp"

namespace fireous {
namespace ui {

// -----------------------------------------------------------------------------------------------------------
// palette / themes

struct Color { double r, g, b; };

struct Theme
{
    Color bg, panelTop, panelBot, edge, text, dim, knobBody, knobCap;
    Color accentEq, accentPeq, accentDelay, accentComp, accentLim, accentGlobal;
    Color lcdText, lcdBg;
    const char* name;
};

static constexpr Theme kThemes[3] = {
    // DBX: black anodized rack, silver knobs, cool LEDs
    {
        {0.045, 0.045, 0.050}, {0.125, 0.125, 0.135}, {0.055, 0.055, 0.062},
        {0.34, 0.34, 0.37}, {0.88, 0.88, 0.86}, {0.48, 0.48, 0.48},
        {0.075, 0.075, 0.085}, {0.72, 0.72, 0.75},
        {0.95, 0.62, 0.18}, {0.78, 0.44, 0.28}, {0.30, 0.62, 0.95},
        {0.30, 0.88, 0.50}, {0.95, 0.30, 0.25}, {0.62, 0.52, 0.38},
        {0.55, 1.00, 0.55}, {0.015, 0.035, 0.025}, "DBX"
    },
    // VOX: cream/ivory panels, dark legends, oxblood/copper accents
    {
        {0.11, 0.09, 0.07}, {0.84, 0.79, 0.66}, {0.63, 0.58, 0.46},
        {0.44, 0.33, 0.19}, {0.12, 0.10, 0.08}, {0.32, 0.26, 0.19},
        {0.17, 0.14, 0.11}, {0.92, 0.88, 0.74},
        {0.66, 0.20, 0.16}, {0.58, 0.35, 0.18}, {0.20, 0.38, 0.48},
        {0.30, 0.48, 0.26}, {0.78, 0.22, 0.15}, {0.48, 0.34, 0.18},
        {1.00, 0.65, 0.15}, {0.10, 0.06, 0.02}, "VOX"
    },
    // HYBRID (default): DBX chassis + VOX cream/copper accents
    {
        {0.050, 0.045, 0.040}, {0.135, 0.125, 0.115}, {0.060, 0.055, 0.050},
        {0.40, 0.30, 0.20}, {0.93, 0.88, 0.76}, {0.52, 0.46, 0.38},
        {0.075, 0.065, 0.060}, {0.78, 0.74, 0.66},
        {0.95, 0.63, 0.20}, {0.74, 0.46, 0.24}, {0.38, 0.58, 0.72},
        {0.36, 0.78, 0.50}, {0.92, 0.34, 0.25}, {0.72, 0.52, 0.30},
        {0.62, 1.00, 0.58}, {0.020, 0.035, 0.025}, "HYBRID"
    },
};

inline const Theme& themeFor(int index) noexcept
{
    return kThemes[std::clamp(index, 0, 2)];
}

inline void setColor(cairo_t* cr, Color c, double a = 1.0) noexcept
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

// -----------------------------------------------------------------------------------------------------------
// actions and UI-only state

enum Action : int {
    kActionNone = 0,
    kActionPresetPrev,
    kActionPresetNext,
    kActionPresetSave,
    kActionPresetDelete,
    kActionScale,
    kActionThemeDbx,
    kActionThemeVox,
    kActionThemeHybrid,
    kActionEqFlat,
    kActionPeqBand1,
    kActionPeqBand2,
    kActionPeqBand3,
    kActionPeqBand4,
    kActionPeqBand5,
    kActionPeqBand6,
    kActionTap,
};

enum LcdKind : int { kLcdHeader = 0, kLcdEq, kLcdPeq, kLcdDelay, kLcdComp, kLcdLim };
enum MeterKind : int { kMeterIn = 0, kMeterOut, kMeterGrComp, kMeterGrLim };

// -----------------------------------------------------------------------------------------------------------
// layout model

static constexpr float kPanelHeaderH = 32.0f;

struct Knob
{
    uint32_t param;
    float cx, cy, radius;
    const char* label;
    Color accent;
};

struct Fader
{
    uint32_t param;
    float x, y, w, h;
    const char* label;
    Color accent;
};

struct SelectorOption
{
    float value;
    const char* label;
};

struct Button
{
    float x, y, w, h;
    const char* label;
    Color accent;
    uint32_t param = kParamCount;
    float value = 0.0f;
    int action = kActionNone;
    bool toggle = false;
};

struct Selector
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr;
};

struct Dropdown
{
    uint32_t param;
    float x, y, w, h;
    std::vector<SelectorOption> options;
    Color accent;
    const char* caption = nullptr;
};

struct ModulePanel
{
    float x, y, w, h;
    int stage;
    const char* title;
    Color accent;
    bool wide;
};

struct Lcd
{
    float x, y, w, h;
    int kind;
    Color text;
    Color bg;
};

struct Meter
{
    float x, y, w, h;
    int kind;
    Color color;
    bool vertical;
    const char* label;
};

struct PresetBar
{
    Button prev, next, save, del, scale;
    float nameX, nameY, nameW, nameH;
    Color accent;
};

struct Layout
{
    float width = 1280.0f, height = 900.0f;
    std::vector<ModulePanel> panels;
    std::vector<Knob> knobs;
    std::vector<Fader> faders;
    std::vector<Button> buttons;
    std::vector<Selector> selectors;
    std::vector<Dropdown> dropdowns;
    std::vector<Lcd> lcds;
    std::vector<Meter> meters;
    PresetBar presetBar;
};

// -----------------------------------------------------------------------------------------------------------
// small helpers

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI_2, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI_2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI_2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI_2);
    cairo_close_path(cr);
}

inline void drawText(cairo_t* cr, const char* text, double x, double y)
{
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

inline void centeredText(cairo_t* cr, const char* text, double cx, double cy)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    drawText(cr, text, cx - ext.width / 2.0 - ext.x_bearing, cy - ext.height / 2.0 - ext.y_bearing);
}

inline void setFont(cairo_t* cr, double size, bool bold = true, bool mono = false)
{
    cairo_select_font_face(cr, mono ? "DejaVu Sans Mono" : "DejaVu Sans",
                           CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

inline float paramToNormalized(uint32_t param, float value) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    float t;
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        const float v = value < lo ? lo : (value > hi ? hi : value);
        t = (float)(std::log(v / lo) / std::log(hi / lo));
    }
    else
    {
        t = (value - info.min) / (info.max - info.min);
    }
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float normalizedToParam(uint32_t param, float t) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        return lo * std::pow(hi / lo, t);
    }
    return info.min + (info.max - info.min) * t;
}

inline void formatParamValue(uint32_t index, float value, char* out, size_t outSize) noexcept
{
    const ParamInfo& info = getParamInfo(index);
    if (std::strcmp(info.unit, "dB") == 0)
    {
        std::snprintf(out, outSize, "%+.1f", value);
    }
    else if (std::strcmp(info.unit, "Hz") == 0)
    {
        if (value >= 1000.0f) std::snprintf(out, outSize, "%.2fk", value / 1000.0f);
        else                  std::snprintf(out, outSize, "%.0f", value);
    }
    else if (std::strcmp(info.unit, "ms") == 0)
    {
        if (value >= 10.0f) std::snprintf(out, outSize, "%.0f", value);
        else                 std::snprintf(out, outSize, "%.1f", value);
    }
    else if (std::strcmp(info.unit, "x") == 0)
    {
        std::snprintf(out, outSize, "%.2fx", value);
    }
    else if (index == kParamCompRatio)
    {
        std::snprintf(out, outSize, "%.1f:1", value);
    }
    else if (index == kParamCompLink)
    {
        std::snprintf(out, outSize, "%.0f%%", value * 100.0f);
    }
    else
    {
        std::snprintf(out, outSize, "%.2f", value);
    }
}

inline void getSelectorOptionRect(const Selector& sel, size_t i, double& ox, double& ow) noexcept
{
    const size_t n = sel.options.size();
    const double gap = 5.0;
    ow = (sel.w - gap * (double)(n > 0 ? n - 1 : 0)) / (double)(n > 0 ? n : 1);
    ox = sel.x + (ow + gap) * (double)i;
}

inline void getDropdownOptionRect(const Dropdown& dd, size_t i, float layoutHeight, double& oy, double& oh) noexcept
{
    const double n = (double)dd.options.size();
    oh = dd.h - 4.0;
    const double totalH = n * oh;
    const double downSpace = (double)layoutHeight - (dd.y + dd.h);
    const double upSpace = dd.y;
    const bool openUpward = (totalH > downSpace) && (upSpace > downSpace);
    double start = openUpward ? (dd.y - totalH) : (dd.y + dd.h);
    const double maxStart = (double)layoutHeight - totalH;
    if (start > maxStart) start = maxStart;
    if (start < 0.0) start = 0.0;
    oy = start + oh * (double)i;
}

// -----------------------------------------------------------------------------------------------------------
// stage helpers

static uint32_t stageOnParam(int stage) noexcept
{
    switch (stage)
    {
    case kStageEq:    return kParamEqOn;
    case kStagePeq:   return kParamPeqOn;
    case kStageDelay: return kParamDelayOn;
    case kStageComp:  return kParamCompOn;
    case kStageLim:   return kParamLimOn;
    default:          return kParamCount;
    }
}

static const char* stageTitle(int stage) noexcept
{
    switch (stage)
    {
    case kStageEq:    return "GRAPHIC EQ";
    case kStagePeq:   return "PARAMETRIC EQ";
    case kStageDelay: return "DELAY";
    case kStageComp:  return "COMPRESSOR";
    case kStageLim:   return "LIMITER";
    default:          return "";
    }
}

static const char* stageShort(int stage) noexcept
{
    switch (stage)
    {
    case kStageEq:    return "EQ";
    case kStagePeq:   return "PEQ";
    case kStageDelay: return "DELAY";
    case kStageComp:  return "COMP";
    case kStageLim:   return "LIMIT";
    default:          return "";
    }
}

static Color stageAccent(const Theme& th, int stage) noexcept
{
    switch (stage)
    {
    case kStageEq:    return th.accentEq;
    case kStagePeq:   return th.accentPeq;
    case kStageDelay: return th.accentDelay;
    case kStageComp:  return th.accentComp;
    case kStageLim:   return th.accentLim;
    default:          return th.accentGlobal;
    }
}

static constexpr float kGraphicFreqsUI[10] = {
    31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

static const char* graphicFreqLabel(int i) noexcept
{
    static constexpr const char* labels[10] = { "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    return labels[std::clamp(i, 0, 9)];
}

static uint32_t peqTypeParam(int band) noexcept
{
    static constexpr uint32_t p[6] = { kParamPeqType1, kParamPeqType2, kParamPeqType3,
                                       kParamPeqType4, kParamPeqType5, kParamPeqType6 };
    return p[std::clamp(band, 0, 5)];
}
static uint32_t peqFreqParam(int band) noexcept
{
    static constexpr uint32_t p[6] = { kParamPeqFreq1, kParamPeqFreq2, kParamPeqFreq3,
                                       kParamPeqFreq4, kParamPeqFreq5, kParamPeqFreq6 };
    return p[std::clamp(band, 0, 5)];
}
static uint32_t peqGainParam(int band) noexcept
{
    static constexpr uint32_t p[6] = { kParamPeqGain1, kParamPeqGain2, kParamPeqGain3,
                                       kParamPeqGain4, kParamPeqGain5, kParamPeqGain6 };
    return p[std::clamp(band, 0, 5)];
}
static uint32_t peqQParam(int band) noexcept
{
    static constexpr uint32_t p[6] = { kParamPeqQ1, kParamPeqQ2, kParamPeqQ3,
                                       kParamPeqQ4, kParamPeqQ5, kParamPeqQ6 };
    return p[std::clamp(band, 0, 5)];
}

static uint32_t eqGainParam(int i) noexcept
{
    static constexpr uint32_t p[10] = { kParamEqGain31, kParamEqGain63, kParamEqGain125,
                                        kParamEqGain250, kParamEqGain500, kParamEqGain1k,
                                        kParamEqGain2k, kParamEqGain4k, kParamEqGain8k,
                                        kParamEqGain16k };
    return p[std::clamp(i, 0, 9)];
}

// -----------------------------------------------------------------------------------------------------------
// layout builder

inline void addKnob(Layout& L, uint32_t param, float cx, float cy, float r,
                    const char* label, Color accent)
{
    L.knobs.push_back({ param, cx, cy, r, label, accent });
}

inline void addFader(Layout& L, uint32_t param, float x, float y, float w, float h,
                     const char* label, Color accent)
{
    L.faders.push_back({ param, x, y, w, h, label, accent });
}

inline Button makeButton(float x, float y, float w, float h, const char* label, Color accent,
                         uint32_t param = kParamCount, float value = 0.0f,
                         int action = kActionNone, bool toggle = false)
{
    Button b;
    b.x = x; b.y = y; b.w = w; b.h = h; b.label = label; b.accent = accent;
    b.param = param; b.value = value; b.action = action; b.toggle = toggle;
    return b;
}

inline void addSelector(Layout& L, uint32_t param, float x, float y, float w, float h,
                        std::vector<SelectorOption> options, Color accent, const char* caption)
{
    L.selectors.push_back({ param, x, y, w, h, std::move(options), accent, caption });
}

inline void addLcd(Layout& L, float x, float y, float w, float h, int kind,
                   Color text, Color bg)
{
    L.lcds.push_back({ x, y, w, h, kind, text, bg });
}

inline void addMeter(Layout& L, float x, float y, float w, float h, int kind,
                     Color color, bool vertical, const char* label)
{
    L.meters.push_back({ x, y, w, h, kind, color, vertical, label });
}

inline void buildEqModule(Layout& L, const ModulePanel& p, const float* values, Color accent, const Theme& th)
{
    const float x = p.x, y = p.y, w = p.w, h = p.h;
    addLcd(L, x + 18, y + 42, w - 36, 52, kLcdEq, th.lcdText, th.lcdBg);
    L.buttons.push_back(makeButton(x + w - 92, y + 98, 72, 22, "FLAT", accent, kParamCount, 0.0f, kActionEqFlat));
    (void)values;

    if (p.wide)
    {
        const float top = y + 140;
        const float bottom = y + h - 26;
        const float left = x + 46;
        const float right = x + w - 46;
        const float step = (right - left) / 9.0f;
        for (int i = 0; i < 10; ++i)
            addFader(L, eqGainParam(i), left + step * (float)i - 18.0f, top, 36.0f,
                     bottom - top, graphicFreqLabel(i), accent);
    }
    else
    {
        for (int row = 0; row < 2; ++row)
        {
            const float top = y + 122 + (float)row * 106.0f;
            const float left = x + 54;
            const float right = x + w - 54;
            const float step = (right - left) / 4.0f;
            for (int i = 0; i < 5; ++i)
            {
                const int band = row * 5 + i;
                addFader(L, eqGainParam(band), left + step * (float)i - 15.0f, top,
                         30.0f, 78.0f, graphicFreqLabel(band), accent);
            }
        }
    }
}

inline void buildPeqModule(Layout& L, const ModulePanel& p, const float* values,
                           int selectedBand, Color accent, const Theme& th)
{
    const float x = p.x, y = p.y, w = p.w, h = p.h;
    addLcd(L, x + 18, y + 42, w - 36, 52, kLcdPeq, th.lcdText, th.lcdBg);

    const float bw = 48.0f, bgap = 6.0f;
    const float total = 6.0f * bw + 5.0f * bgap;
    const float bx = x + (w - total) * 0.5f;
    static constexpr int actions[6] = { kActionPeqBand1, kActionPeqBand2, kActionPeqBand3,
                                        kActionPeqBand4, kActionPeqBand5, kActionPeqBand6 };
    for (int i = 0; i < 6; ++i)
    {
        L.buttons.push_back(makeButton(bx + (float)i * (bw + bgap), y + 104, bw, 22,
                                       i == 0 ? "B1" : i == 1 ? "B2" : i == 2 ? "B3"
                                       : i == 3 ? "B4" : i == 4 ? "B5" : "B6",
                                       accent, kParamCount, 0.0f, actions[i]));
    }

    const int band = std::clamp(selectedBand, 0, 5);
    addKnob(L, peqFreqParam(band), x + w * 0.25f, y + 174, 26, "FREQ", accent);
    addKnob(L, peqGainParam(band), x + w * 0.50f, y + 174, 26, "GAIN", accent);
    addKnob(L, peqQParam(band),    x + w * 0.75f, y + 174, 26, "Q", accent);

    addSelector(L, peqTypeParam(band), x + 18, y + 238, w - 36, 22,
                { {0.0f, "BELL"}, {1.0f, "LOW"}, {2.0f, "HIGH"} }, accent, "TYPE");

    const float fy = y + 286;
    L.buttons.push_back(makeButton(x + 18, fy, 44, 22, "HP", accent, kParamHpOn, 0.0f, kActionNone, true));
    addKnob(L, kParamHpFreq, x + 92, fy + 10, 14, "FREQ", accent);
    addSelector(L, kParamHpSlope, x + 120, fy, 70, 20, { {0.0f, "12"}, {1.0f, "24"} }, accent, nullptr);

    const float rx = x + w * 0.5f + 10;
    L.buttons.push_back(makeButton(rx, fy, 44, 22, "LP", accent, kParamLpOn, 0.0f, kActionNone, true));
    addKnob(L, kParamLpFreq, rx + 74, fy + 10, 14, "FREQ", accent);
    addSelector(L, kParamLpSlope, rx + 102, fy, 70, 20, { {0.0f, "12"}, {1.0f, "24"} }, accent, nullptr);
    (void)values;
    (void)h;
}

inline void buildDelayModule(Layout& L, const ModulePanel& p, float* /*values*/, Color accent, const Theme& th)
{
    const float x = p.x, y = p.y, w = p.w, h = p.h;
    addLcd(L, x + 18, y + 42, w - 36, 52, kLcdDelay, th.lcdText, th.lcdBg);

    L.dropdowns.push_back({ kParamDelaySync, x + 18, y + 104, 128, 24,
                            { {0.0f, "Free"}, {1.0f, "1/1"}, {2.0f, "1/2."}, {3.0f, "1/2"},
                              {4.0f, "1/2T"}, {5.0f, "1/4."}, {6.0f, "1/4"}, {7.0f, "1/4T"},
                              {8.0f, "1/8."}, {9.0f, "1/8"}, {10.0f, "1/8T"}, {11.0f, "1/16."},
                              {12.0f, "1/16"}, {13.0f, "1/16T"}, {14.0f, "1/32"}, {15.0f, "1/64"},
                              {16.0f, "1/128"} },
                            accent, "SYNC" });

    addSelector(L, kParamDelayMode, x + 158, y + 104, w - 176, 24,
                { {0.0f, "EFFECT"}, {1.0f, "ALIGN"}, {2.0f, "BOTH"} }, accent, "MODE");

    addKnob(L, kParamDelayTimeMs, x + 60, y + 176, 22, "TIME", accent);
    addKnob(L, kParamDelayFeedback, x + 150, y + 176, 22, "FDBK", accent);
    addKnob(L, kParamDelayPingPong, x + 240, y + 176, 22, "P-PONG", accent);
    addKnob(L, kParamDelayMix, x + 330, y + 176, 22, "MIX", accent);

    addKnob(L, kParamAlignLMs, x + 98, y + 258, 17, "ALIGN L", accent);
    addKnob(L, kParamAlignRMs, x + 208, y + 258, 17, "ALIGN R", accent);
    L.buttons.push_back(makeButton(x + 292, y + 250, 84, 24, "TAP", accent, kParamCount, 0.0f, kActionTap));
    (void)h;
}

inline void buildCompModule(Layout& L, const ModulePanel& p, Color accent, const Theme& th)
{
    const float x = p.x, y = p.y, w = p.w, h = p.h;
    addLcd(L, x + 18, y + 42, w - 100, 52, kLcdComp, th.lcdText, th.lcdBg);
    addMeter(L, x + w - 38, y + 104, 16, h - 154, kMeterGrComp, th.accentComp, true, "GR");

    addKnob(L, kParamCompThreshold, x + 44, y + 174, 20, "THRESH", accent);
    addKnob(L, kParamCompRatio,     x + 114, y + 174, 20, "RATIO", accent);
    addKnob(L, kParamCompAttack,    x + 184, y + 174, 20, "ATTACK", accent);
    addKnob(L, kParamCompRelease,   x + 254, y + 174, 20, "RELEASE", accent);
    addKnob(L, kParamCompMakeup,    x + 324, y + 174, 20, "MAKEUP", accent);
    addKnob(L, kParamCompLink,      x + 330, y + 268, 13, "LINK", accent);

    addSelector(L, kParamCompDetector, x + 18, y + 258, 90, 22,
                { {0.0f, "RMS"}, {1.0f, "PEAK"} }, accent, nullptr);
    L.buttons.push_back(makeButton(x + 116, y + 258, 86, 22, "AUTO MK", accent,
                                   kParamCompAutoMakeup, 0.0f, kActionNone, true));
    addSelector(L, kParamCompKey, x + 210, y + 258, 86, 22,
                { {0.0f, "INT"}, {1.0f, "EXT"} }, accent, nullptr);
    (void)h;
}

inline void buildLimModule(Layout& L, const ModulePanel& p, Color accent, const Theme& th)
{
    const float x = p.x, y = p.y, w = p.w, h = p.h;
    addLcd(L, x + 18, y + 42, w - 100, 52, kLcdLim, th.lcdText, th.lcdBg);
    addMeter(L, x + w - 38, y + 104, 16, h - 154, kMeterGrLim, th.accentLim, true, "GR");

    addKnob(L, kParamLimCeiling, x + 100, y + 174, 26, "CEILING", accent);
    addKnob(L, kParamLimRelease, x + 240, y + 174, 26, "RELEASE", accent);
    L.buttons.push_back(makeButton(x + 18, y + 252, 102, 24, "TRUE PEAK", accent,
                                   kParamLimTruePeak, 0.0f, kActionNone, true));
    addSelector(L, kParamLimLookahead, x + 132, y + 252, 170, 24,
                { {0.0f, "0 ms"}, {1.0f, "1 ms"}, {2.0f, "2 ms"} }, accent, "LOOKAHEAD");
    (void)h;
}

inline Layout buildLayout(float width, float height, const float* values,
                          int selectedPeqBand, int themeIndex)
{
    Layout L;
    L.width = width;
    L.height = height;
    const Theme& th = themeFor(themeIndex);

    // header preset bar
    {
        PresetBar& pb = L.presetBar;
        pb.accent = th.accentGlobal;
        const float y = 28.0f, h = 42.0f;
        float x = 690.0f;
        pb.prev = makeButton(x, y, 32, h, "<", th.accentGlobal, kParamCount, 0.0f, kActionPresetPrev);
        x += 38.0f;
        pb.next = makeButton(x, y, 32, h, ">", th.accentGlobal, kParamCount, 0.0f, kActionPresetNext);
        x += 40.0f;
        pb.nameX = x; pb.nameY = y; pb.nameW = 200.0f; pb.nameH = h;
        x += pb.nameW + 8.0f;
        pb.save = makeButton(x, y, 64, h, "SAVE", th.accentGlobal, kParamCount, 0.0f, kActionPresetSave);
        x += 70.0f;
        pb.del = makeButton(x, y, 74, h, "DELETE", th.accentLim, kParamCount, 0.0f, kActionPresetDelete);
        x += 80.0f;
        pb.scale = makeButton(x, y, 64, h, "", th.accentGlobal, kParamCount, 0.0f, kActionScale);
    }

    // header LCD
    addLcd(L, 370, 28, 300, 42, kLcdHeader, th.lcdText, th.lcdBg);

    // global strip: phase, latency, theme, delta, bypass, meters
    L.buttons.push_back(makeButton(80, 86, 54, 24, "IIR", th.accentGlobal, kParamPhaseMode, 0.0f));
    L.buttons.push_back(makeButton(140, 86, 70, 24, "LINEAR", th.accentGlobal, kParamPhaseMode, 1.0f));
    addSelector(L, kParamLinearLatency, 244, 86, 170, 24,
                { {0.0f, "2.7"}, {1.0f, "5"}, {2.0f, "10"}, {3.0f, "20"}, {4.0f, "40"} },
                th.accentGlobal, nullptr);
    L.buttons.push_back(makeButton(458, 86, 54, 24, "DBX", th.accentGlobal, kParamCount, 0.0f, kActionThemeDbx));
    L.buttons.push_back(makeButton(518, 86, 54, 24, "VOX", th.accentGlobal, kParamCount, 0.0f, kActionThemeVox));
    L.buttons.push_back(makeButton(578, 86, 54, 24, "HYB", th.accentGlobal, kParamCount, 0.0f, kActionThemeHybrid));
    L.buttons.push_back(makeButton(650, 86, 62, 24, "DELTA", th.accentGlobal, kParamMonitor, 0.0f, kActionNone, true));
    L.buttons.push_back(makeButton(720, 86, 70, 24, "BYPASS", th.accentLim, kParamBypass, 0.0f, kActionNone, true));
    addMeter(L, 810, 92, 170, 10, kMeterIn, th.accentComp, false, "IN");
    addMeter(L, 1004, 92, 170, 10, kMeterOut, th.accentGlobal, false, "OUT");

    // module grid
    const float left = 24.0f, right = width - 24.0f;
    const float gap = 12.0f;
    const float gridY = 166.0f;
    const float gridBottom = height - 16.0f;
    const float rowH = (gridBottom - gridY - gap) * 0.5f;
    const float cw = (right - left - 2.0f * gap) / 3.0f;
    const float wideW = 2.0f * cw + gap;

    const float chainPos[5] = {
        values[kParamChainPosEq], values[kParamChainPosPeq], values[kParamChainPosDelay],
        values[kParamChainPosComp], values[kParamChainPosLim]
    };
    int order[5];
    resolveChainOrder(chainPos, order);

    struct Rect { float x, y, w, h; };
    const Rect slots[5] = {
        { left, gridY, wideW, rowH },
        { left + wideW + gap, gridY, cw, rowH },
        { left, gridY + rowH + gap, cw, rowH },
        { left + cw + gap, gridY + rowH + gap, cw, rowH },
        { left + 2.0f * (cw + gap), gridY + rowH + gap, cw, rowH },
    };

    for (int i = 0; i < 5; ++i)
    {
        const int stage = order[i];
        const Rect& r = slots[i];
        const Color accent = stageAccent(th, stage);
        const bool wide = (i == 0);
        L.panels.push_back({ r.x, r.y, r.w, r.h, stage, stageTitle(stage), accent, wide });

        L.buttons.push_back(makeButton(r.x + r.w - 74, r.y + 6, 58, 22, "ON", accent,
                                       stageOnParam(stage), 0.0f, kActionNone, true));

        switch (stage)
        {
        case kStageEq:    buildEqModule(L, L.panels.back(), values, accent, th); break;
        case kStagePeq:   buildPeqModule(L, L.panels.back(), values, selectedPeqBand, accent, th); break;
        case kStageDelay: buildDelayModule(L, L.panels.back(), nullptr, accent, th); break;
        case kStageComp:  buildCompModule(L, L.panels.back(), accent, th); break;
        case kStageLim:   buildLimModule(L, L.panels.back(), accent, th); break;
        default: break;
        }
    }

    return L;
}

// -----------------------------------------------------------------------------------------------------------
// paint

struct PaintState
{
    const float* values = nullptr;
    int themeIndex = 2;
    int selectedPeqBand = 1;
    double sampleRate = 48000.0;

    int hoverKnob = -1;
    int dragKnob = -1;
    int hoverFader = -1;
    int dragFader = -1;
    int hoverButton = -1;
    int hoverSelector = -1;
    int hoverSelectorOption = -1;
    int hoverDropdown = -1;
    int openDropdown = -1;
    int hoverDropdownOption = -1;

    int hoverPanel = -1;
    int dragPanel = -1;
    int dropSlot = -1;
    double dragGhostX = 0.0;
    double dragGhostY = 0.0;

    const char* presetName = "INIT";
    bool presetIsUnsaved = false;
    bool presetEditingName = false;
    const char* presetEditBuffer = "";
    int hoverPresetButton = -1;
    const char* scaleLabel = "";
    bool presetDeleteEnabled = false;
};

inline void paintButtonEx(cairo_t* cr, const Button& b, bool active, bool hovered, const Theme& th);
inline void paintButton(cairo_t* cr, const Button& b, const PaintState& s, int index, const Theme& th);
inline void paintPresetBar(cairo_t* cr, const PresetBar& pb, const PaintState& s, const Theme& th);
inline void paintLcd(cairo_t* cr, const Lcd& l, const PaintState& s, const Theme& th);

inline void paintChassis(cairo_t* cr, const Layout& L, const Theme& th)
{
    cairo_set_source_rgb(cr, th.bg.r, th.bg.g, th.bg.b);
    cairo_paint(cr);

    // subtle horizontal brushed-metal lines
    for (int i = 0; i < 220; ++i)
    {
        const double y = (double)i * L.height / 220.0;
        const double a = 0.010 + 0.012 * (0.5 + 0.5 * std::sin((double)i * 12.9898));
        setColor(cr, th.text, a);
        cairo_rectangle(cr, 0, y, L.width, 1);
        cairo_fill(cr);
    }

    roundedRect(cr, 8, 8, L.width - 16, L.height - 16, 10);
    setColor(cr, th.edge, 0.8);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);

    // rack ears
    for (int side = 0; side < 2; ++side)
    {
        const double x = side == 0 ? 16.0 : L.width - 56.0;
        cairo_set_source_rgb(cr, th.panelTop.r * 0.8, th.panelTop.g * 0.8, th.panelTop.b * 0.8);
        roundedRect(cr, x, 20, 40, 58, 6);
        cairo_fill(cr);
        for (int i = 0; i < 3; ++i)
        {
            const double yy = 32.0 + i * 17.0;
            setColor(cr, (Color){0.02, 0.02, 0.02});
            cairo_arc(cr, x + 20, yy, 5.5, 0, 2 * M_PI);
            cairo_fill(cr);
            setColor(cr, th.edge, 0.8);
            cairo_arc(cr, x + 20, yy, 5.0, 0, 2 * M_PI);
            cairo_set_line_width(cr, 1);
            cairo_stroke(cr);
        }
    }

    // header panel
    cairo_pattern_t* g = cairo_pattern_create_linear(0, 20, 0, 78);
    cairo_pattern_add_color_stop_rgb(g, 0, th.panelTop.r, th.panelTop.g, th.panelTop.b);
    cairo_pattern_add_color_stop_rgb(g, 1, th.panelBot.r, th.panelBot.g, th.panelBot.b);
    roundedRect(cr, 62, 20, L.width - 124, 58, 8);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    setColor(cr, th.edge, 0.7);
    cairo_set_line_width(cr, 1.2);
    roundedRect(cr, 62.5, 20.5, L.width - 125, 57, 8);
    cairo_stroke(cr);
}

inline void paintHeader(cairo_t* cr, const Layout& L, const PaintState& s, const Theme& th)
{
    setFont(cr, 30);
    setColor(cr, th.text);
    drawText(cr, "FIReous", 82, 54);
    setFont(cr, 10, true, true);
    setColor(cr, th.accentPeq);
    drawText(cr, "FIR-5", 238, 38);
    setFont(cr, 8, false, true);
    setColor(cr, th.dim);
    drawText(cr, "IIR / LINEAR-PHASE", 238, 54);

    setColor(cr, th.accentComp);
    cairo_arc(cr, 342, 44, 4.5, 0, 2 * M_PI);
    cairo_fill(cr);
    setFont(cr, 8, true, true);
    setColor(cr, th.dim);
    drawText(cr, "PWR", 352, 48);

    // header LCD
    for (const Lcd& l : L.lcds)
    {
        if (l.kind != kLcdHeader)
            continue;
        paintLcd(cr, l, s, th);
    }

    // preset bar
    paintPresetBar(cr, L.presetBar, s, th);
}

inline void paintGlobalStrip(cairo_t* cr, const Layout& L, const PaintState& s, const Theme& th)
{
    roundedRect(cr, 24, 84, L.width - 48, 34, 4.0);
    setColor(cr, th.panelBot, 0.55);
    cairo_fill_preserve(cr);
    setColor(cr, th.edge, 0.45);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    setFont(cr, 8, true, true);
    setColor(cr, th.dim);
    drawText(cr, "PHASE", 34, 103);
    drawText(cr, "LAT", 216, 103);
    drawText(cr, "THEME", 430, 103);
    (void)s;
}

inline void paintSignalFlow(cairo_t* cr, const Layout& L, const PaintState& s, const Theme& th)
{
    setFont(cr, 8, true, true);
    setColor(cr, th.dim);
    drawText(cr, "SIGNAL FLOW", 34, 144);

    const float y = 126.0f, h = 26.0f, bw = 118.0f, gap = 24.0f;
    float x = 168.0f;
    char names[7][16];
    std::snprintf(names[0], sizeof(names[0]), "IN");
    for (int i = 0; i < 5; ++i)
        std::snprintf(names[i + 1], sizeof(names[i + 1]), "%s", stageShort(L.panels[(size_t)i].stage));
    std::snprintf(names[6], sizeof(names[6]), "OUT");

    for (int i = 0; i < 7; ++i)
    {
        const bool edge = (i == 0 || i == 6);
        bool bypassed = false;
        if (!edge && s.values)
        {
            const int stage = L.panels[(size_t)(i - 1)].stage;
            bypassed = s.values[stageOnParam(stage)] < 0.5f;
        }
        roundedRect(cr, x, y, bw, h, 4);
        setColor(cr, th.panelTop, 0.9);
        cairo_fill_preserve(cr);
        setColor(cr, edge ? th.edge : (bypassed ? th.dim : th.accentGlobal), 0.8);
        cairo_set_line_width(cr, 1.1);
        cairo_stroke(cr);

        setColor(cr, edge ? th.dim : (bypassed ? th.dim : th.accentComp));
        cairo_arc(cr, x + 12, y + h / 2, 3.2, 0, 2 * M_PI);
        cairo_fill(cr);

        setFont(cr, 9, true, true);
        setColor(cr, edge ? th.dim : th.text);
        drawText(cr, names[i], x + 24, y + 17);

        if (i < 6)
        {
            const double ax = x + bw + 4.0, ay = y + h / 2;
            setColor(cr, th.dim, 0.7);
            cairo_set_line_width(cr, 1.4);
            cairo_move_to(cr, ax, ay);
            cairo_line_to(cr, ax + gap - 10, ay);
            cairo_stroke(cr);
            cairo_move_to(cr, ax + gap - 14, ay - 4);
            cairo_line_to(cr, ax + gap - 10, ay);
            cairo_line_to(cr, ax + gap - 14, ay + 4);
            cairo_stroke(cr);
        }
        x += bw + gap;
    }
}

inline void paintKnob(cairo_t* cr, const Knob& k, float value, bool hovered, bool dragging,
                      const Theme& th)
{
    const float t = paramToNormalized(k.param, value);
    const double start = M_PI * 0.75;
    const double sweep = M_PI * 1.5;
    const double ang = start + sweep * t;

    for (int i = 0; i < 11; ++i)
    {
        const double a = start + sweep * (double)i / 10.0;
        const double x0 = k.cx + std::cos(a) * (k.radius + 6.0);
        const double y0 = k.cy + std::sin(a) * (k.radius + 6.0);
        const double x1 = k.cx + std::cos(a) * (k.radius + 9.0);
        const double y1 = k.cy + std::sin(a) * (k.radius + 9.0);
        setColor(cr, th.dim, 0.7);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, x0, y0);
        cairo_line_to(cr, x1, y1);
        cairo_stroke(cr);
    }

    setColor(cr, th.knobBody);
    cairo_set_line_width(cr, 4.0);
    cairo_arc(cr, k.cx, k.cy, k.radius, start, start + sweep);
    cairo_stroke(cr);
    setColor(cr, k.accent, dragging ? 1.0 : (hovered ? 0.9 : 0.8));
    cairo_set_line_width(cr, 4.0);
    cairo_arc(cr, k.cx, k.cy, k.radius, start, ang);
    cairo_stroke(cr);

    cairo_pattern_t* body = cairo_pattern_create_radial(k.cx - k.radius * 0.35,
                                                        k.cy - k.radius * 0.4,
                                                        k.radius * 0.2,
                                                        k.cx, k.cy, k.radius);
    cairo_pattern_add_color_stop_rgb(body, 0, th.knobCap.r, th.knobCap.g, th.knobCap.b);
    cairo_pattern_add_color_stop_rgb(body, 1, th.knobBody.r, th.knobBody.g, th.knobBody.b);
    cairo_arc(cr, k.cx, k.cy, k.radius - 3.0, 0, 2 * M_PI);
    cairo_set_source(cr, body);
    cairo_fill(cr);
    cairo_pattern_destroy(body);
    setColor(cr, th.edge, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_arc(cr, k.cx, k.cy, k.radius - 3.0, 0, 2 * M_PI);
    cairo_stroke(cr);

    setColor(cr, th.knobCap);
    cairo_arc(cr, k.cx, k.cy, k.radius * 0.55, 0, 2 * M_PI);
    cairo_fill(cr);
    setColor(cr, k.accent);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, k.cx + std::cos(ang) * k.radius * 0.18,
                  k.cy + std::sin(ang) * k.radius * 0.18);
    cairo_line_to(cr, k.cx + std::cos(ang) * k.radius * 0.55,
                  k.cy + std::sin(ang) * k.radius * 0.55);
    cairo_stroke(cr);

    char buf[32];
    formatParamValue(k.param, value, buf, sizeof(buf));
    setFont(cr, 8, true, true);
    setColor(cr, k.accent);
    centeredText(cr, buf, k.cx, k.cy - k.radius - 12.0);
    setFont(cr, 8, true);
    setColor(cr, th.dim);
    centeredText(cr, k.label, k.cx, k.cy + k.radius + 15.0);
}

inline void paintFader(cairo_t* cr, const Fader& f, float value, bool hovered, bool dragging,
                       const Theme& th)
{
    const float t = paramToNormalized(f.param, value);
    roundedRect(cr, f.x + f.w * 0.5f - 4.0, f.y, 8.0, f.h, 4.0);
    setColor(cr, (Color){0.02, 0.02, 0.02});
    cairo_fill(cr);
    setColor(cr, th.edge, 0.6);
    cairo_set_line_width(cr, 1.0);
    roundedRect(cr, f.x + f.w * 0.5f - 4.0, f.y, 8.0, f.h, 4.0);
    cairo_stroke(cr);

    for (int i = 0; i < 9; ++i)
    {
        const double ty = f.y + f.h * (double)i / 8.0;
        setColor(cr, th.dim, 0.55);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, f.x + f.w * 0.5f + 8.0, ty);
        cairo_line_to(cr, f.x + f.w * 0.5f + 14.0, ty);
        cairo_stroke(cr);
    }

    const double cy = f.y + (1.0 - t) * (f.h - 24.0) + 12.0;
    cairo_pattern_t* g = cairo_pattern_create_linear(0, cy - 11, 0, cy + 11);
    cairo_pattern_add_color_stop_rgb(g, 0, th.knobCap.r, th.knobCap.g, th.knobCap.b);
    cairo_pattern_add_color_stop_rgb(g, 1, th.knobBody.r, th.knobBody.g, th.knobBody.b);
    roundedRect(cr, f.x + f.w * 0.5f - 15.0, cy - 11.0, 30.0, 22.0, 4.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);
    setColor(cr, dragging ? f.accent : th.edge, dragging || hovered ? 1.0 : 0.8);
    cairo_set_line_width(cr, 1.0);
    roundedRect(cr, f.x + f.w * 0.5f - 15.0, cy - 11.0, 30.0, 22.0, 4.0);
    cairo_stroke(cr);
    setColor(cr, th.text);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, f.x + f.w * 0.5f - 9.0, cy);
    cairo_line_to(cr, f.x + f.w * 0.5f + 9.0, cy);
    cairo_stroke(cr);

    setFont(cr, 7, true, true);
    setColor(cr, th.dim);
    centeredText(cr, f.label, f.x + f.w * 0.5f, f.y + f.h + 13.0);
}

inline bool buttonActive(const Button& b, const PaintState& s)
{
    if (b.action >= kActionThemeDbx && b.action <= kActionThemeHybrid)
        return s.themeIndex == (b.action - kActionThemeDbx);
    if (b.action >= kActionPeqBand1 && b.action <= kActionPeqBand6)
        return s.selectedPeqBand == (b.action - kActionPeqBand1);
    if (b.toggle && b.param < kParamCount && s.values)
        return s.values[b.param] > 0.5f;
    if (b.param < kParamCount && s.values)
        return std::fabs(s.values[b.param] - b.value) < 0.01f;
    return false;
}

inline void paintButtonEx(cairo_t* cr, const Button& b, bool active, bool hovered,
                          const Theme& th)
{
    cairo_pattern_t* g = cairo_pattern_create_linear(0, b.y, 0, b.y + b.h);
    cairo_pattern_add_color_stop_rgb(g, 0, std::min(1.0, th.panelTop.r * 1.3),
                                        std::min(1.0, th.panelTop.g * 1.3),
                                        std::min(1.0, th.panelTop.b * 1.3));
    cairo_pattern_add_color_stop_rgb(g, 1, th.panelBot.r, th.panelBot.g, th.panelBot.b);
    roundedRect(cr, b.x, b.y, b.w, b.h, 4.0);
    cairo_set_source(cr, g);
    cairo_fill(cr);
    cairo_pattern_destroy(g);

    setColor(cr, active ? b.accent : th.edge, active ? 0.95 : (hovered ? 0.9 : 0.55));
    cairo_set_line_width(cr, active ? 1.8 : 1.1);
    roundedRect(cr, b.x, b.y, b.w, b.h, 4.0);
    cairo_stroke(cr);

    if (active)
    {
        setColor(cr, b.accent, 0.16);
        roundedRect(cr, b.x + 1, b.y + 1, b.w - 2, b.h - 2, 3.5);
        cairo_fill(cr);
    }

    setFont(cr, b.h > 30 ? 11 : 8, true, true);
    setColor(cr, active ? b.accent : th.text);
    centeredText(cr, b.label, b.x + b.w / 2.0, b.y + b.h / 2.0);
}

inline void paintButton(cairo_t* cr, const Button& b, const PaintState& s, int index,
                        const Theme& th)
{
    paintButtonEx(cr, b, buttonActive(b, s), index == s.hoverButton, th);
}

inline void paintSelector(cairo_t* cr, const Selector& sel, float value, int hoverOption,
                          const Theme& th)
{
    if (sel.caption)
    {
        setFont(cr, 7, true, true);
        setColor(cr, th.dim);
        drawText(cr, sel.caption, sel.x, sel.y - 5);
    }
    for (size_t i = 0; i < sel.options.size(); ++i)
    {
        double ox, ow;
        getSelectorOptionRect(sel, i, ox, ow);
        const bool active = std::fabs(sel.options[i].value - value) < 0.01f;
        const bool hovered = (int)i == hoverOption;
        roundedRect(cr, ox, sel.y, ow, sel.h, 3.0);
        setColor(cr, active ? sel.accent : th.panelTop, active ? 0.9 : (hovered ? 1.0 : 0.8));
        cairo_fill_preserve(cr);
        setColor(cr, active ? sel.accent : th.edge, 0.8);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
        setFont(cr, 7.5, true, true);
        setColor(cr, active ? th.bg : th.text);
        centeredText(cr, sel.options[i].label, ox + ow / 2.0, sel.y + sel.h / 2.0);
    }
}

inline void paintDropdownClosed(cairo_t* cr, const Dropdown& dd, float value, bool hovered,
                                bool open, const Theme& th)
{
    if (dd.caption)
    {
        setFont(cr, 7, true, true);
        setColor(cr, th.dim);
        drawText(cr, dd.caption, dd.x, dd.y - 5);
    }
    roundedRect(cr, dd.x, dd.y, dd.w, dd.h, 3.0);
    setColor(cr, th.panelTop, (hovered || open) ? 1.0 : 0.85);
    cairo_fill_preserve(cr);
    setColor(cr, open ? dd.accent : th.edge, 0.8);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    const char* label = "?";
    for (const auto& o : dd.options)
        if (std::fabs(o.value - value) < 0.01f)
            label = o.label;
    setFont(cr, 7.5, true, true);
    setColor(cr, dd.accent);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, label, &ext);
    drawText(cr, label, dd.x + 8.0, dd.y + dd.h / 2.0 - ext.height / 2.0 - ext.y_bearing);

    const double ax = dd.x + dd.w - 12.0, ay = dd.y + dd.h / 2.0;
    setColor(cr, th.dim);
    cairo_move_to(cr, ax - 4, open ? ay + 2 : ay - 2);
    cairo_line_to(cr, ax + 4, open ? ay + 2 : ay - 2);
    cairo_line_to(cr, ax, open ? ay - 2 : ay + 2);
    cairo_close_path(cr);
    cairo_fill(cr);
}

inline void paintDropdownOpenList(cairo_t* cr, const Dropdown& dd, float value,
                                  int hoverOption, float layoutHeight, const Theme& th)
{
    const size_t n = dd.options.size();
    double oyFirst, oyLast, oh;
    getDropdownOptionRect(dd, 0, layoutHeight, oyFirst, oh);
    getDropdownOptionRect(dd, n - 1, layoutHeight, oyLast, oh);
    const double top = std::min(oyFirst, oyLast);
    const double bottom = std::max(oyFirst, oyLast) + oh;

    setColor(cr, th.bg, 0.96);
    cairo_rectangle(cr, dd.x, top, dd.w, bottom - top);
    cairo_fill(cr);
    setColor(cr, dd.accent, 0.8);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, dd.x, top, dd.w, bottom - top);
    cairo_stroke(cr);

    for (size_t i = 0; i < n; ++i)
    {
        double oy;
        getDropdownOptionRect(dd, i, layoutHeight, oy, oh);
        const bool active = std::fabs(dd.options[i].value - value) < 0.01f;
        const bool hovered = (int)i == hoverOption;
        cairo_rectangle(cr, dd.x, oy, dd.w, oh);
        if (active) setColor(cr, dd.accent, 0.9);
        else        setColor(cr, th.panelTop, hovered ? 1.0 : 0.0);
        cairo_fill(cr);
        setFont(cr, 7.5, true, true);
        setColor(cr, active ? th.bg : th.text);
        drawText(cr, dd.options[i].label, dd.x + 8.0, oy + oh / 2.0 + 2.5);
    }
}

inline void paintMeter(cairo_t* cr, const Meter& m, float value, const Theme& th)
{
    setFont(cr, 7, true, true);
    setColor(cr, th.dim);
    if (m.label)
    {
        if (m.vertical)
            centeredText(cr, m.label, m.x + m.w * 0.5, m.y - 4.0);
        else
            drawText(cr, m.label, m.x - 18.0, m.y + m.h);
    }

    const int segments = m.vertical ? 12 : 20;
    if (m.vertical)
    {
        const double gap = 2.0;
        const double sh = (m.h - gap * (segments - 1)) / segments;
        for (int i = 0; i < segments; ++i)
        {
            const double frac = (double)(i + 1) / segments;
            Color col = th.accentComp;
            if (i >= segments * 0.8) col = th.accentLim;
            else if (i >= segments * 0.6) col = th.accentEq;
            const double yy = m.y + m.h - (i + 1) * sh - i * gap;
            roundedRect(cr, m.x, yy, m.w, sh, 1.2);
            setColor(cr, col, value >= frac ? 0.95 : 0.10);
            cairo_fill(cr);
        }
    }
    else
    {
        roundedRect(cr, m.x, m.y, m.w, m.h, 2.0);
        setColor(cr, th.panelTop, 0.8);
        cairo_fill(cr);
        roundedRect(cr, m.x, m.y, m.w * std::clamp(value, 0.0f, 1.0f), m.h, 2.0);
        setColor(cr, value > 0.9f ? th.accentLim : m.color, 0.9);
        cairo_fill(cr);
    }
}

inline void paintLcd(cairo_t* cr, const Lcd& l, const PaintState& s, const Theme& th)
{
    roundedRect(cr, l.x, l.y, l.w, l.h, 4.0);
    setColor(cr, l.bg);
    cairo_fill_preserve(cr);
    setColor(cr, th.edge, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    for (int i = 0; i < (int)l.h; i += 3)
    {
        setColor(cr, l.text, 0.025);
        cairo_rectangle(cr, l.x + 3, l.y + i, l.w - 6, 1);
        cairo_fill(cr);
    }

    char buf[64];
    if (l.kind == kLcdHeader)
    {
        float lat = s.values ? s.values[kParamMeterLatency] : 0.0f;
        if (lat <= 0.0f && s.values && s.values[kParamPhaseMode] > 0.5f)
            lat = linearLatencyMsFromIndex((int)std::lround(s.values[kParamLinearLatency])) *
                  0.001f * (float)s.sampleRate;
        const float latMs = s.sampleRate > 0.0 ? lat * 1000.0f / (float)s.sampleRate : 0.0f;
        std::snprintf(buf, sizeof(buf), "LATENCY %.1f ms", latMs);
        setFont(cr, 13, true, true);
        setColor(cr, l.text);
        drawText(cr, buf, l.x + 12, l.y + 21);
        const bool linear = s.values && s.values[kParamPhaseMode] > 0.5f;
        std::snprintf(buf, sizeof(buf), "%s  ·  %.1f kHz", linear ? "LINEAR" : "IIR",
                      s.sampleRate / 1000.0);
        setFont(cr, 8, false, true);
        setColor(cr, l.text, 0.75);
        drawText(cr, buf, l.x + 12, l.y + 35);
        return;
    }

    // faint grid
    for (int i = 1; i < 6; ++i)
    {
        const double xx = l.x + 6 + (l.w - 12) * i / 6.0;
        setColor(cr, l.text, 0.08);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, xx, l.y + 4);
        cairo_line_to(cr, xx, l.y + l.h - 4);
        cairo_stroke(cr);
    }

    if (l.kind == kLcdEq || l.kind == kLcdPeq)
    {
        const int bands = (l.kind == kLcdEq) ? 10 : 6;
        const double cx0 = l.x + 8.0, cx1 = l.x + l.w - 8.0;
        const double cy = l.y + l.h * 0.55;
        const double scale = (l.h - 18.0) / 24.0;
        cairo_move_to(cr, cx0, cy);
        for (int i = 0; i < 80; ++i)
        {
            const double t = (double)i / 79.0;
            const double f = 20.0 * std::pow(1000.0, t);
            double gain = 0.0;
            for (int b = 0; b < bands; ++b)
            {
                const double fb = (l.kind == kLcdEq)
                                ? (double)kGraphicFreqsUI[b]
                                : (double)s.values[peqFreqParam(b)];
                const double gb = (l.kind == kLcdEq)
                                ? (double)s.values[eqGainParam(b)]
                                : (double)s.values[peqGainParam(b)];
                const double d = std::log2(f / fb) / 0.75;
                gain += gb * std::exp(-0.5 * d * d);
            }
            gain = std::clamp(gain, -12.0, 12.0);
            const double xx = cx0 + (cx1 - cx0) * t;
            const double yy = cy - gain * scale;
            if (i == 0) cairo_move_to(cr, xx, yy);
            else        cairo_line_to(cr, xx, yy);
        }
        setColor(cr, l.text, 0.95);
        cairo_set_line_width(cr, 1.6);
        cairo_stroke(cr);
        setFont(cr, 7, true, true);
        setColor(cr, l.text, 0.8);
        drawText(cr, l.kind == kLcdEq ? "10-BAND MAG" : "6-BAND MAG", l.x + 8, l.y + 12);
        if (l.kind == kLcdPeq)
        {
            std::snprintf(buf, sizeof(buf), "B%d", s.selectedPeqBand + 1);
            drawText(cr, buf, l.x + l.w - 28, l.y + 12);
        }
        return;
    }

    if (l.kind == kLcdDelay)
    {
        const float ms = s.values ? s.values[kParamDelayTimeMs] : 250.0f;
        std::snprintf(buf, sizeof(buf), "%.0f ms", ms);
        setFont(cr, 17, true, true);
        setColor(cr, l.text);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        drawText(cr, buf, l.x + l.w - 10 - ext.width, l.y + 30);
        std::snprintf(buf, sizeof(buf), "ALIGN L %.1f  R %.1f",
                      s.values ? s.values[kParamAlignLMs] : 0.0f,
                      s.values ? s.values[kParamAlignRMs] : 0.0f);
        setFont(cr, 7, false, true);
        setColor(cr, l.text, 0.75);
        drawText(cr, buf, l.x + 8, l.y + l.h - 7);
        setFont(cr, 7, true, true);
        drawText(cr, "STEREO DELAY", l.x + 8, l.y + 12);
        return;
    }

    if (l.kind == kLcdComp)
    {
        const float gr = s.values ? s.values[kParamMeterGrComp] : 0.0f;
        std::snprintf(buf, sizeof(buf), "-%.1f dB", gr);
        setFont(cr, 16, true, true);
        setColor(cr, l.text);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        drawText(cr, buf, l.x + l.w - 10 - ext.width, l.y + 28);
        setFont(cr, 7, false, true);
        std::snprintf(buf, sizeof(buf), "%s  ·  LINK %.0f%%",
                      s.values && s.values[kParamCompDetector] > 0.5f ? "PEAK" : "RMS",
                      s.values ? s.values[kParamCompLink] * 100.0f : 100.0f);
        drawText(cr, buf, l.x + 8, l.y + l.h - 7);
        setFont(cr, 7, true, true);
        drawText(cr, "GR", l.x + 8, l.y + 12);
        return;
    }

    if (l.kind == kLcdLim)
    {
        const float ceil = s.values ? s.values[kParamLimCeiling] : -0.3f;
        std::snprintf(buf, sizeof(buf), "%.1f dB", ceil);
        setFont(cr, 16, true, true);
        setColor(cr, l.text);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, buf, &ext);
        drawText(cr, buf, l.x + l.w - 10 - ext.width, l.y + 28);
        setFont(cr, 7, false, true);
        std::snprintf(buf, sizeof(buf), "%s  ·  LA %.0f ms",
                      s.values && s.values[kParamLimTruePeak] > 0.5f ? "TRUE" : "SAMPLE",
                      s.values ? s.values[kParamLimLookahead] : 0.0f);
        drawText(cr, buf, l.x + 8, l.y + l.h - 7);
        setFont(cr, 7, true, true);
        drawText(cr, "CEILING", l.x + 8, l.y + 12);
        return;
    }
}

inline void paintPresetBar(cairo_t* cr, const PresetBar& pb, const PaintState& s, const Theme& th)
{
    paintButtonEx(cr, pb.prev, false, s.hoverPresetButton == 0, th);
    paintButtonEx(cr, pb.next, false, s.hoverPresetButton == 1, th);
    paintButtonEx(cr, pb.save, false, s.hoverPresetButton == 2, th);
    paintButtonEx(cr, pb.del, false, s.hoverPresetButton == 3, th);
    Button scale = pb.scale;
    scale.label = s.scaleLabel;
    paintButtonEx(cr, scale, false, s.hoverPresetButton == 4, th);

    roundedRect(cr, pb.nameX, pb.nameY, pb.nameW, pb.nameH, 4.0);
    setColor(cr, th.panelBot, 0.95);
    cairo_fill_preserve(cr);
    setColor(cr, s.presetEditingName ? pb.accent : th.edge, s.presetEditingName ? 1.0 : 0.7);
    cairo_set_line_width(cr, s.presetEditingName ? 1.8 : 1.0);
    cairo_stroke(cr);

    setFont(cr, 7, true, true);
    setColor(cr, th.dim);
    drawText(cr, "PRESET", pb.nameX + 10, pb.nameY + 13);
    setFont(cr, 11, true);
    if (s.presetEditingName)
    {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s_", s.presetEditBuffer);
        setColor(cr, th.text);
        drawText(cr, buf, pb.nameX + 10, pb.nameY + pb.nameH - 10);
    }
    else
    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s%s", s.presetName, s.presetIsUnsaved ? " *" : "");
        setColor(cr, s.presetIsUnsaved ? th.accentPeq : th.text);
        drawText(cr, buf, pb.nameX + 10, pb.nameY + pb.nameH - 10);
    }
}

inline void paint(cairo_t* cr, const Layout& L, const PaintState& s)
{
    const Theme& th = themeFor(s.themeIndex);
    paintChassis(cr, L, th);
    paintHeader(cr, L, s, th);
    paintGlobalStrip(cr, L, s, th);
    paintSignalFlow(cr, L, s, th);

    // drop target highlight
    if (s.dropSlot >= 0 && s.dropSlot < 5)
    {
        const ModulePanel& p = L.panels[(size_t)s.dropSlot];
        roundedRect(cr, p.x - 3, p.y - 3, p.w + 6, p.h + 6, 10);
        setColor(cr, p.accent, 0.9);
        cairo_set_line_width(cr, 2.5);
        cairo_stroke(cr);
    }

    for (const ModulePanel& p : L.panels)
    {
        cairo_pattern_t* g = cairo_pattern_create_linear(0, p.y, 0, p.y + p.h);
        cairo_pattern_add_color_stop_rgb(g, 0, th.panelTop.r, th.panelTop.g, th.panelTop.b);
        cairo_pattern_add_color_stop_rgb(g, 1, th.panelBot.r, th.panelBot.g, th.panelBot.b);
        roundedRect(cr, p.x, p.y, p.w, p.h, 7.0);
        cairo_set_source(cr, g);
        cairo_fill_preserve(cr);
        cairo_pattern_destroy(g);
        setColor(cr, p.accent, 0.75);
        cairo_set_line_width(cr, 1.2);
        cairo_stroke(cr);

        // header strip
        roundedRect(cr, p.x + 1, p.y + 1, p.w - 2, kPanelHeaderH - 1, 6.0);
        setColor(cr, th.panelTop, 0.55);
        cairo_fill(cr);
        setColor(cr, p.accent, 0.9);
        cairo_rectangle(cr, p.x + 1, p.y + 1, 4, kPanelHeaderH - 1);
        cairo_fill(cr);

        setFont(cr, 11, true);
        setColor(cr, th.text);
        drawText(cr, p.title, p.x + 14, p.y + 21);
        setFont(cr, 8, true, true);
        setColor(cr, th.dim);
        char slot[8];
        std::snprintf(slot, sizeof(slot), "%02d", (int)(&p - L.panels.data()) + 1);
        drawText(cr, slot, p.x + p.w - 108, p.y + 20);

        // drag dots
        setColor(cr, th.dim, 0.7);
        for (int i = 0; i < 3; ++i)
        {
            cairo_arc(cr, p.x + p.w - 16, p.y + 9 + i * 5, 1.4, 0, 2 * M_PI);
            cairo_fill(cr);
        }

        // screws
        setColor(cr, th.edge, 0.6);
        cairo_arc(cr, p.x + 9, p.y + p.h - 9, 4.0, 0, 2 * M_PI);
        cairo_stroke(cr);
        cairo_arc(cr, p.x + p.w - 9, p.y + p.h - 9, 4.0, 0, 2 * M_PI);
        cairo_stroke(cr);
    }

    for (const Lcd& l : L.lcds)
    {
        if (l.kind == kLcdHeader)
            continue;
        paintLcd(cr, l, s, th);
    }

    for (size_t i = 0; i < L.faders.size(); ++i)
    {
        const Fader& f = L.faders[i];
        paintFader(cr, f, s.values[f.param], (int)i == s.hoverFader, (int)i == s.dragFader, th);
    }

    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        paintKnob(cr, k, s.values[k.param], (int)i == s.hoverKnob, (int)i == s.dragKnob, th);
    }

    for (size_t i = 0; i < L.selectors.size(); ++i)
    {
        const Selector& sel = L.selectors[i];
        paintSelector(cr, sel, s.values[sel.param],
                      (int)i == s.hoverSelector ? s.hoverSelectorOption : -1, th);
    }

    for (size_t i = 0; i < L.dropdowns.size(); ++i)
    {
        const Dropdown& dd = L.dropdowns[i];
        paintDropdownClosed(cr, dd, s.values[dd.param],
                            (int)i == s.hoverDropdown, (int)i == s.openDropdown, th);
    }

    for (size_t i = 0; i < L.buttons.size(); ++i)
        paintButton(cr, L.buttons[i], s, (int)i == s.hoverButton ? (int)i : -1, th);

    for (const Meter& m : L.meters)
    {
        float value = 0.0f;
        if (m.kind == kMeterIn)
            value = std::max(s.values[kParamMeterInPeakL], s.values[kParamMeterInPeakR]);
        else if (m.kind == kMeterOut)
            value = std::max(s.values[kParamMeterOutPeakL], s.values[kParamMeterOutPeakR]);
        else if (m.kind == kMeterGrComp)
            value = std::clamp(s.values[kParamMeterGrComp] / 24.0f, 0.0f, 1.0f);
        else if (m.kind == kMeterGrLim)
            value = std::clamp(s.values[kParamMeterGrLim] / 24.0f, 0.0f, 1.0f);
        paintMeter(cr, m, value, th);
    }

    if (s.openDropdown >= 0 && (size_t)s.openDropdown < L.dropdowns.size())
    {
        const Dropdown& dd = L.dropdowns[s.openDropdown];
        paintDropdownOpenList(cr, dd, s.values[dd.param], s.hoverDropdownOption, L.height, th);
    }

    if (s.dragPanel >= 0 && (size_t)s.dragPanel < L.panels.size())
    {
        const ModulePanel& p = L.panels[(size_t)s.dragPanel];
        roundedRect(cr, s.dragGhostX, s.dragGhostY, p.w, p.h, 8.0);
        setColor(cr, th.panelTop, 0.85);
        cairo_fill_preserve(cr);
        setColor(cr, p.accent, 0.95);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
        setFont(cr, 11, true);
        setColor(cr, p.accent);
        drawText(cr, p.title, s.dragGhostX + 14, s.dragGhostY + 22);
    }
}

// -----------------------------------------------------------------------------------------------------------
// hit testing

inline int hitTestKnob(const Layout& L, double x, double y) noexcept
{
    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        const double dx = x - k.cx, dy = y - k.cy;
        const double reach = k.radius + 8.0;
        if (dx * dx + dy * dy <= reach * reach)
            return (int)i;
    }
    return -1;
}

inline int hitTestFader(const Layout& L, double x, double y) noexcept
{
    for (size_t i = 0; i < L.faders.size(); ++i)
    {
        const Fader& f = L.faders[i];
        if (x >= f.x + f.w * 0.5 - 22 && x <= f.x + f.w * 0.5 + 22 &&
            y >= f.y - 8 && y <= f.y + f.h + 8)
            return (int)i;
    }
    return -1;
}

inline int hitTestButton(const Layout& L, double x, double y) noexcept
{
    for (size_t i = 0; i < L.buttons.size(); ++i)
    {
        const Button& b = L.buttons[i];
        if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h)
            return (int)i;
    }
    return -1;
}

inline bool hitTestSelector(const Layout& L, double x, double y, int& selIdx, int& optIdx) noexcept
{
    selIdx = -1;
    optIdx = -1;
    for (size_t i = 0; i < L.selectors.size(); ++i)
    {
        const Selector& sel = L.selectors[i];
        if (x < sel.x || x > sel.x + sel.w || y < sel.y || y > sel.y + sel.h)
            continue;
        for (size_t j = 0; j < sel.options.size(); ++j)
        {
            double ox, ow;
            getSelectorOptionRect(sel, j, ox, ow);
            if (x >= ox && x <= ox + ow)
            {
                selIdx = (int)i;
                optIdx = (int)j;
                return true;
            }
        }
    }
    return false;
}

inline int hitTestDropdownClosed(const Layout& L, double x, double y) noexcept
{
    for (size_t i = 0; i < L.dropdowns.size(); ++i)
    {
        const Dropdown& d = L.dropdowns[i];
        if (x >= d.x && x <= d.x + d.w && y >= d.y && y <= d.y + d.h)
            return (int)i;
    }
    return -1;
}

inline int hitTestDropdownOption(const Dropdown& d, double x, double y, float layoutHeight) noexcept
{
    if (x < d.x || x > d.x + d.w)
        return -1;
    for (size_t i = 0; i < d.options.size(); ++i)
    {
        double oy, oh;
        getDropdownOptionRect(d, i, layoutHeight, oy, oh);
        if (y >= oy && y <= oy + oh)
            return (int)i;
    }
    return -1;
}

inline int hitTestPanelHeader(const Layout& L, double x, double y) noexcept
{
    for (size_t i = 0; i < L.panels.size(); ++i)
    {
        const ModulePanel& p = L.panels[i];
        if (x >= p.x && x <= p.x + p.w && y >= p.y && y <= p.y + kPanelHeaderH)
            return (int)i;
    }
    return -1;
}

inline int hitTestPanel(const Layout& L, double x, double y) noexcept
{
    for (size_t i = 0; i < L.panels.size(); ++i)
    {
        const ModulePanel& p = L.panels[i];
        if (x >= p.x && x <= p.x + p.w && y >= p.y && y <= p.y + p.h)
            return (int)i;
    }
    return -1;
}

} // namespace ui
} // namespace fireous
