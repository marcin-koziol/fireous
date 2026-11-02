/*
 * FIReous - custom Cairo hardware-DSP UI.
 *
 * Layout/painting lives in ui/UIPainter.hpp; this file only does DPF mouse
 * handling, preset CRUD, UI scale, theme selection and reorder.
 */

#include "DistrhoUI.hpp"
#include "UiScale.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#include "Params.hpp"
#include "ui/FactoryPresets.hpp"
#include "ui/PresetStore.hpp"
#include "ui/UIPainter.hpp"

START_NAMESPACE_DISTRHO

using namespace fireous;
using namespace fireous::ui;

// -----------------------------------------------------------------------------------------------------------

class FireousUI : public UI
{
public:
    FireousUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        applyScale();

        fThemeIndex = loadTheme();

        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i] = getParamInfo(i).def;

        fLayout = ui::buildLayout((float)DISTRHO_UI_DEFAULT_WIDTH,
                                  (float)DISTRHO_UI_DEFAULT_HEIGHT,
                                  fValues, fSelectedPeqBand, fThemeIndex);

        fPresetNames = ui::listPresets();
        if (fPresetNames.empty())
        {
            ui::seedFactoryPresets();
            fPresetNames = ui::listPresets();
        }
    }

protected:
    // ---------------------------------------------------------------------

    void setParameterValue(uint32_t index, float value)
    {
        UI::setParameterValue(index, paramToHost(index, value));
    }

    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParamCount)
            return;

        if (isOutputParam(index))
        {
            fValues[index] = value;
            repaint();
            return;
        }

        value = paramFromHost(index, value);

        const float tol = 1e-3f * std::max(1.0f, std::fabs(fValues[index]));
        if (std::fabs(value - fValues[index]) > tol)
        {
            if (!fLoadingPreset && index != kParamMonitor)
                fDirty = true;
            fValues[index] = value;

            if (isChainPosParam(index))
                rebuildLayout();

            repaint();
        }
    }

    void uiIdle() override
    {
        if (!fHostSizeSynced)
        {
            fHostSizeSynced = true;
            setSize(getWidth(), getHeight());
        }
    }

    void onCairoDisplay(const CairoGraphicsContext& context) override
    {
        fireous_uiscale::formatLabel(fScaleSetting, fScale, fScaleLabel, sizeof(fScaleLabel));

        ui::PaintState state;
        state.values = fValues;
        state.themeIndex = fThemeIndex;
        state.selectedPeqBand = fSelectedPeqBand;
        state.sampleRate = getSampleRate();

        state.hoverKnob = fHoverKnob;
        state.dragKnob = fDragKnob;
        state.hoverFader = fHoverFader;
        state.dragFader = fDragFader;
        state.hoverButton = fHoverButton;
        state.hoverSelector = fHoverSelector;
        state.hoverSelectorOption = fHoverSelectorOption;
        state.hoverDropdown = fHoverDropdown;
        state.openDropdown = fOpenDropdown;
        state.hoverDropdownOption = fHoverDropdownOption;

        state.hoverPanel = fHoverPanel;
        state.dragPanel = fDragPanel;
        state.dropSlot = fDropSlot;
        state.dragGhostX = fDragCurrentX - fDragGrabDX;
        state.dragGhostY = fDragCurrentY - fDragGrabDY;

        state.presetName = fCurrentName.c_str();
        state.presetIsUnsaved = fDirty;
        state.presetEditingName = fEditingName;
        state.presetEditBuffer = fEditBuffer.c_str();
        state.hoverPresetButton = fHoverPresetButton;
        state.presetDeleteEnabled = fPresetIndex >= 0;
        state.scaleLabel = fScaleLabel;

        cairo_t* const cr = context.handle;
        cairo_save(cr);
        cairo_scale(cr, fScale, fScale);
        ui::paint(cr, fLayout, state);
        cairo_restore(cr);
    }

    // ---------------------------------------------------------------------
    // input

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        const double mx = ev.pos.getX() / fScale;
        const double my = ev.pos.getY() / fScale;

        if (ev.press)
        {
            const int presetBtn = hitTestPresetButton(mx, my);
            if (presetBtn >= 0)
            {
                switch (presetBtn)
                {
                case 0: fEditingName = false; presetStep(-1); break;
                case 1: fEditingName = false; presetStep(1); break;
                case 2: presetSave(); break;
                case 3: fEditingName = false; presetDelete(); break;
                case 4: fEditingName = false; cycleScale(); break;
                default: break;
                }
                repaint();
                return true;
            }

            if (hitTestPresetName(mx, my))
            {
                fEditingName = true;
                fEditBuffer = fCurrentName;
                repaint();
                return true;
            }

            if (fEditingName)
            {
                fEditingName = false;
                repaint();
            }

            if (fOpenDropdown >= 0)
            {
                const ui::Dropdown& dd = fLayout.dropdowns[(size_t)fOpenDropdown];
                const int opt = ui::hitTestDropdownOption(dd, mx, my, fLayout.height);
                if (opt >= 0)
                    setParam(dd.param, dd.options[(size_t)opt].value);
                fOpenDropdown = -1;
                repaint();
                return true;
            }

            const int ddIdx = ui::hitTestDropdownClosed(fLayout, mx, my);
            if (ddIdx >= 0)
            {
                fOpenDropdown = ddIdx;
                repaint();
                return true;
            }

            int selIdx = -1, optIdx = -1;
            if (ui::hitTestSelector(fLayout, mx, my, selIdx, optIdx))
            {
                const ui::Selector& sel = fLayout.selectors[(size_t)selIdx];
                setParam(sel.param, sel.options[(size_t)optIdx].value);
                repaint();
                return true;
            }

            const int btnIdx = ui::hitTestButton(fLayout, mx, my);
            if (btnIdx >= 0)
            {
                handleButton(btnIdx);
                repaint();
                return true;
            }

            const int panelIdx = ui::hitTestPanelHeader(fLayout, mx, my);
            if (panelIdx >= 0)
            {
                fDragPanel = panelIdx;
                fDropSlot = panelIdx;
                fDragGrabDX = mx - fLayout.panels[(size_t)panelIdx].x;
                fDragGrabDY = my - fLayout.panels[(size_t)panelIdx].y;
                fDragCurrentX = mx;
                fDragCurrentY = my;
                repaint();
                return true;
            }

            const int faderIdx = ui::hitTestFader(fLayout, mx, my);
            if (faderIdx >= 0)
            {
                const ui::Fader& f = fLayout.faders[(size_t)faderIdx];
                const auto now = std::chrono::steady_clock::now();
                const bool dbl = fLastClickFader == faderIdx &&
                                 std::chrono::duration_cast<std::chrono::milliseconds>(now - fLastClickTime).count() < 350;
                fLastClickFader = faderIdx;
                fLastClickTime = now;
                if (dbl)
                {
                    setParam(f.param, getParamInfo(f.param).def);
                    fDragFader = -1;
                }
                else
                {
                    fDragFader = faderIdx;
                    fDragStartY = my;
                    fDragStartT = ui::paramToNormalized(f.param, fValues[f.param]);
                }
                repaint();
                return true;
            }

            const int knobIdx = ui::hitTestKnob(fLayout, mx, my);
            if (knobIdx >= 0)
            {
                const ui::Knob& k = fLayout.knobs[(size_t)knobIdx];
                const auto now = std::chrono::steady_clock::now();
                const bool dbl = fLastClickKnob == knobIdx &&
                                 std::chrono::duration_cast<std::chrono::milliseconds>(now - fLastClickTime).count() < 350;
                fLastClickKnob = knobIdx;
                fLastClickTime = now;
                if (dbl)
                {
                    setParam(k.param, getParamInfo(k.param).def);
                    fDragKnob = -1;
                }
                else
                {
                    fDragKnob = knobIdx;
                    fDragStartY = my;
                    fDragStartT = ui::paramToNormalized(k.param, fValues[k.param]);
                }
                repaint();
                return true;
            }

            return false;
        }

        if (fDragKnob >= 0 || fDragFader >= 0)
        {
            fDragKnob = -1;
            fDragFader = -1;
            repaint();
            return true;
        }

        if (fDragPanel >= 0)
        {
            reorderChain(fDragPanel, fDropSlot);
            fDragPanel = -1;
            fDropSlot = -1;
            repaint();
            return true;
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const double mx = ev.pos.getX() / fScale;
        const double my = ev.pos.getY() / fScale;

        if (fDragKnob >= 0)
        {
            const ui::Knob& k = fLayout.knobs[(size_t)fDragKnob];
            float t = fDragStartT + (float)((fDragStartY - my) / 200.0);
            t = std::clamp(t, 0.0f, 1.0f);
            setParam(k.param, ui::normalizedToParam(k.param, t));
            repaint();
            return true;
        }

        if (fDragFader >= 0)
        {
            const ui::Fader& f = fLayout.faders[(size_t)fDragFader];
            float t = fDragStartT + (float)((fDragStartY - my) / 180.0);
            t = std::clamp(t, 0.0f, 1.0f);
            setParam(f.param, ui::normalizedToParam(f.param, t));
            repaint();
            return true;
        }

        if (fDragPanel >= 0)
        {
            fDragCurrentX = mx;
            fDragCurrentY = my;
            const int slot = ui::hitTestPanel(fLayout, mx, my);
            fDropSlot = slot >= 0 ? slot : fDropSlot;
            repaint();
            return true;
        }

        if (fOpenDropdown >= 0)
        {
            const ui::Dropdown& dd = fLayout.dropdowns[(size_t)fOpenDropdown];
            const int opt = ui::hitTestDropdownOption(dd, mx, my, fLayout.height);
            if (opt != fHoverDropdownOption)
            {
                fHoverDropdownOption = opt;
                repaint();
            }
            return true;
        }

        const int newKnob = ui::hitTestKnob(fLayout, mx, my);
        const int newFader = ui::hitTestFader(fLayout, mx, my);
        const int newButton = ui::hitTestButton(fLayout, mx, my);
        int selIdx = -1, optIdx = -1;
        ui::hitTestSelector(fLayout, mx, my, selIdx, optIdx);
        const int newDropdown = ui::hitTestDropdownClosed(fLayout, mx, my);
        const int newPanel = ui::hitTestPanelHeader(fLayout, mx, my);
        const int newPreset = hitTestPresetButton(mx, my);

        if (newKnob != fHoverKnob || newFader != fHoverFader || newButton != fHoverButton ||
            selIdx != fHoverSelector || optIdx != fHoverSelectorOption ||
            newDropdown != fHoverDropdown || newPanel != fHoverPanel || newPreset != fHoverPresetButton)
        {
            fHoverKnob = newKnob;
            fHoverFader = newFader;
            fHoverButton = newButton;
            fHoverSelector = selIdx;
            fHoverSelectorOption = optIdx;
            fHoverDropdown = newDropdown;
            fHoverPanel = newPanel;
            fHoverPresetButton = newPreset;
            repaint();
        }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        if (fOpenDropdown >= 0 || fDragPanel >= 0)
            return false;

        const double mx = ev.pos.getX() / fScale;
        const double my = ev.pos.getY() / fScale;
        const int knobIdx = ui::hitTestKnob(fLayout, mx, my);
        const int faderIdx = ui::hitTestFader(fLayout, mx, my);
        const float step = ev.delta.getY() > 0.0 ? 0.02f : -0.02f;

        if (knobIdx >= 0)
        {
            const ui::Knob& k = fLayout.knobs[(size_t)knobIdx];
            const float t = std::clamp(ui::paramToNormalized(k.param, fValues[k.param]) + step, 0.0f, 1.0f);
            setParam(k.param, ui::normalizedToParam(k.param, t));
            repaint();
            return true;
        }
        if (faderIdx >= 0)
        {
            const ui::Fader& f = fLayout.faders[(size_t)faderIdx];
            const float t = std::clamp(ui::paramToNormalized(f.param, fValues[f.param]) + step, 0.0f, 1.0f);
            setParam(f.param, ui::normalizedToParam(f.param, t));
            repaint();
            return true;
        }
        return false;
    }

    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (!fEditingName)
            return false;
        if (!ev.press)
            return true;

        if (ev.key == kKeyEnter || ev.key == kKeyPadEnter)
        {
            fCurrentName = ui::sanitizePresetName(fEditBuffer);
            fEditingName = false;
            repaint();
            return true;
        }
        if (ev.key == kKeyEscape)
        {
            fEditingName = false;
            repaint();
            return true;
        }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete)
        {
            if (!fEditBuffer.empty())
                fEditBuffer.pop_back();
            repaint();
            return true;
        }
        return true;
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (!fEditingName)
            return false;
        if (ev.character >= 32 && fEditBuffer.size() < 32)
            fEditBuffer += ev.string;
        repaint();
        return true;
    }

private:
    void setParam(uint32_t index, float value)
    {
        if (index >= kParamCount || isOutputParam(index))
            return;
        editParameter(index, true);
        UI::setParameterValue(index, paramToHost(index, value));
        editParameter(index, false);
        fValues[index] = value;
        if (!fLoadingPreset && index != kParamMonitor)
            fDirty = true;
    }

    static bool isChainPosParam(uint32_t index) noexcept
    {
        return index == kParamChainPosEq || index == kParamChainPosPeq ||
               index == kParamChainPosDelay || index == kParamChainPosComp ||
               index == kParamChainPosLim;
    }

    void rebuildLayout()
    {
        fLayout = ui::buildLayout((float)DISTRHO_UI_DEFAULT_WIDTH,
                                  (float)DISTRHO_UI_DEFAULT_HEIGHT,
                                  fValues, fSelectedPeqBand, fThemeIndex);
    }

    void handleButton(int index)
    {
        if (index < 0 || (size_t)index >= fLayout.buttons.size())
            return;
        const ui::Button& b = fLayout.buttons[(size_t)index];

        switch (b.action)
        {
        case kActionPresetPrev: presetStep(-1); return;
        case kActionPresetNext: presetStep(1); return;
        case kActionPresetSave: presetSave(); return;
        case kActionPresetDelete: presetDelete(); return;
        case kActionScale: cycleScale(); return;
        case kActionThemeDbx: setTheme(0); return;
        case kActionThemeVox: setTheme(1); return;
        case kActionThemeHybrid: setTheme(2); return;
        case kActionEqFlat:
            for (int i = 0; i < 10; ++i)
                setParam(eqGainParam(i), 0.0f);
            fDirty = true;
            return;
        case kActionPeqBand1:
        case kActionPeqBand2:
        case kActionPeqBand3:
        case kActionPeqBand4:
        case kActionPeqBand5:
        case kActionPeqBand6:
            fSelectedPeqBand = b.action - kActionPeqBand1;
            rebuildLayout();
            return;
        case kActionTap:
            tapTempo();
            return;
        default:
            break;
        }

        if (b.param < kParamCount)
        {
            const float newValue = b.toggle ? (fValues[b.param] > 0.5f ? 0.0f : 1.0f) : b.value;
            setParam(b.param, newValue);
            if (b.param == kParamPhaseMode)
                rebuildLayout();
        }
    }

    void tapTempo()
    {
        const auto now = std::chrono::steady_clock::now();
        if (fLastTap.time_since_epoch().count() != 0)
        {
            const double ms = std::chrono::duration<double, std::milli>(now - fLastTap).count();
            if (ms > 100.0 && ms < 2000.0)
            {
                setParam(kParamDelaySync, 0.0f);
                setParam(kParamDelayTimeMs, (float)ms);
                fDirty = true;
            }
        }
        fLastTap = now;
    }

    // ---------------------------------------------------------------------
    // theme

    int loadTheme() const
    {
        std::ifstream in(themePath());
        int value = 2;
        if (!(in >> value))
            return 2;
        return std::clamp(value, 0, 2);
    }

    void saveTheme(int value) const
    {
#if defined(_WIN32)
        _mkdir(fireous_uiscale::configDirectory().c_str());
#else
        mkdir(fireous_uiscale::configDirectory().c_str(), 0755);
#endif
        std::ofstream out(themePath());
        if (out.is_open())
            out << value << "\n";
    }

    static std::string themePath()
    {
#if defined(_WIN32)
        return fireous_uiscale::configDirectory() + "\\theme";
#else
        return fireous_uiscale::configDirectory() + "/theme";
#endif
    }

    void setTheme(int index)
    {
        fThemeIndex = std::clamp(index, 0, 2);
        saveTheme(fThemeIndex);
        rebuildLayout();
        repaint();
    }

    // ---------------------------------------------------------------------
    // presets

    void refreshPresetList()
    {
        fPresetNames = ui::listPresets();
        fPresetIndex = -1;
        for (size_t i = 0; i < fPresetNames.size(); ++i)
            if (fPresetNames[i] == fCurrentName)
                fPresetIndex = (int)i;
    }

    void loadPresetAt(int index)
    {
        if (index < 0 || index >= (int)fPresetNames.size())
        {
            fCurrentName = "INIT";
            fPresetIndex = -1;
            for (uint32_t i = 0; i < kParamCount; ++i)
                if (!isOutputParam(i))
                    fValues[i] = getParamInfo(i).def;
            fLoadingPreset = true;
            for (uint32_t i = 0; i < kParamCount; ++i)
                if (!isOutputParam(i) && i != kParamMonitor)
                    UI::setParameterValue(i, paramToHost(i, fValues[i]));
            fLoadingPreset = false;
            fDirty = false;
            rebuildLayout();
            repaint();
            return;
        }

        fLoadingPreset = true;
        ui::loadPreset(fPresetNames[(size_t)index], fValues);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (ui::shouldSaveParam(i))
                UI::setParameterValue(i, paramToHost(i, fValues[i]));
        fLoadingPreset = false;

        fPresetIndex = index;
        fCurrentName = fPresetNames[(size_t)index];
        fDirty = false;
        rebuildLayout();
        repaint();
    }

    void presetStep(int delta)
    {
        if (fPresetNames.empty())
            return;
        if (fPresetIndex < 0)
            loadPresetAt(delta < 0 ? (int)fPresetNames.size() - 1 : 0);
        else
        {
            int next = fPresetIndex + delta;
            const int n = (int)fPresetNames.size();
            next = (next % n + n) % n;
            loadPresetAt(next);
        }
    }

    void presetSave()
    {
        std::string name = fEditingName ? fEditBuffer : fCurrentName;
        name = ui::sanitizePresetName(name);
        if (name.empty() || name == "INIT")
            name = "New Preset";

        ui::savePreset(name, fValues);
        fCurrentName = name;
        fEditingName = false;
        fDirty = false;
        refreshPresetList();
        repaint();
    }

    void presetDelete()
    {
        if (fPresetIndex < 0)
            return;
        const int oldIndex = fPresetIndex;
        ui::deletePreset(fCurrentName);
        refreshPresetList();
        const int n = (int)fPresetNames.size();
        if (n == 0)
            loadPresetAt(-1);
        else
            loadPresetAt(std::min(oldIndex, n - 1));
    }

    // ---------------------------------------------------------------------
    // reorder

    void reorderChain(int from, int to)
    {
        if (from < 0 || from >= 5)
            return;
        to = std::clamp(to, 0, 4);

        float cps[5] = {
            fValues[kParamChainPosEq], fValues[kParamChainPosPeq], fValues[kParamChainPosDelay],
            fValues[kParamChainPosComp], fValues[kParamChainPosLim]
        };
        int order[5];
        resolveChainOrder(cps, order);

        const int stage = order[from];
        int list[4];
        int n = 0;
        for (int i = 0; i < 5; ++i)
            if (i != from)
                list[n++] = order[i];

        int result[5];
        for (int i = 0; i < to; ++i)
            result[i] = list[i];
        result[to] = stage;
        for (int i = to + 1; i < 5; ++i)
            result[i] = list[i - 1];

        for (int i = 0; i < 5; ++i)
            setParam(kChainParamForStage[result[i]], (float)i);

        rebuildLayout();
    }

    // ---------------------------------------------------------------------
    // UI scale

    void applyScale()
    {
        fScale = fireous_uiscale::resolve(fScaleSetting, getScaleFactor());
        const uint w = (uint)std::lround(DISTRHO_UI_DEFAULT_WIDTH * fScale);
        const uint h = (uint)std::lround(DISTRHO_UI_DEFAULT_HEIGHT * fScale);
        setGeometryConstraints(w, h, true, false);
        setSize(w, h);
    }

    void cycleScale()
    {
        fScaleSetting = fireous_uiscale::nextSetting(fScaleSetting);
        fireous_uiscale::saveSetting(fScaleSetting);
        applyScale();
        repaint();
    }

    // ---------------------------------------------------------------------
    // hit tests

    static bool insideButton(const ui::Button& b, double x, double y) noexcept
    {
        return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
    }

    int hitTestPresetButton(double x, double y) const noexcept
    {
        const ui::PresetBar& pb = fLayout.presetBar;
        if (insideButton(pb.prev, x, y)) return 0;
        if (insideButton(pb.next, x, y)) return 1;
        if (insideButton(pb.save, x, y)) return 2;
        if (insideButton(pb.del, x, y) && fPresetIndex >= 0) return 3;
        if (insideButton(pb.scale, x, y)) return 4;
        return -1;
    }

    bool hitTestPresetName(double x, double y) const noexcept
    {
        const ui::PresetBar& pb = fLayout.presetBar;
        return x >= pb.nameX && x <= pb.nameX + pb.nameW &&
               y >= pb.nameY && y <= pb.nameY + pb.nameH;
    }

    ui::Layout fLayout;
    float fValues[kParamCount];

    float fScaleSetting = fireous_uiscale::loadSetting();
    double fScale = 1.0;
    char fScaleLabel[24] = {};
    bool fHostSizeSynced = false;

    int fThemeIndex = 2;
    int fSelectedPeqBand = 1;

    int fHoverKnob = -1, fDragKnob = -1;
    int fHoverFader = -1, fDragFader = -1;
    int fHoverButton = -1;
    int fHoverSelector = -1, fHoverSelectorOption = -1;
    int fHoverDropdown = -1, fOpenDropdown = -1, fHoverDropdownOption = -1;
    int fHoverPanel = -1, fDragPanel = -1, fDropSlot = -1;
    double fDragGrabDX = 0.0, fDragGrabDY = 0.0;
    double fDragCurrentX = 0.0, fDragCurrentY = 0.0;
    double fDragStartY = 0.0;
    float fDragStartT = 0.0f;

    int fLastClickKnob = -1;
    int fLastClickFader = -1;
    std::chrono::steady_clock::time_point fLastClickTime{};
    std::chrono::steady_clock::time_point fLastTap{};

    std::vector<std::string> fPresetNames;
    int fPresetIndex = -1;
    std::string fCurrentName = "INIT";
    bool fDirty = false;
    bool fLoadingPreset = false;
    bool fEditingName = false;
    std::string fEditBuffer;
    int fHoverPresetButton = -1;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireousUI)
};

// -----------------------------------------------------------------------------------------------------------

UI* createUI()
{
    return new FireousUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
