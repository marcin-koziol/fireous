// Throwaway dev aid: render the Fireous Cairo UI to a PNG without a display.
// Compile ad hoc:
//   g++ -std=c++17 $(pkg-config --cflags --libs cairo) -o /tmp/fireous_render \
//       tools/offline_render.cpp
//   /tmp/fireous_render /tmp/fireous.png 2

#include "../plugins/FIReous/ui/UIPainter.hpp"

#include <cairo.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

int main(int argc, char** argv)
{
    const char* outPath = argc > 1 ? argv[1] : "/tmp/fireous-ui.png";
    const int theme = argc > 2 ? std::atoi(argv[2]) : 2;

    float values[fireous::kParamCount];
    for (uint32_t i = 0; i < fireous::kParamCount; ++i)
        values[i] = fireous::getParamInfo(i).def;

    // Nice illustrative state for the static mockup.
    values[fireous::kParamEqGain63] = 2.0f;
    values[fireous::kParamEqGain125] = 1.5f;
    values[fireous::kParamEqGain500] = -2.0f;
    values[fireous::kParamEqGain4k] = 2.5f;
    values[fireous::kParamEqGain16k] = 1.0f;

    values[fireous::kParamPeqGain2] = 4.0f;
    values[fireous::kParamPeqFreq2] = 450.0f;
    values[fireous::kParamPeqGain4] = 2.0f;
    values[fireous::kParamPeqFreq4] = 2800.0f;

    values[fireous::kParamDelayOn] = 1.0f;
    values[fireous::kParamDelaySync] = 9.0f;
    values[fireous::kParamDelayTimeMs] = 250.0f;
    values[fireous::kParamAlignLMs] = 1.5f;
    values[fireous::kParamAlignRMs] = 0.5f;

    values[fireous::kParamCompOn] = 1.0f;
    values[fireous::kParamLimOn] = 1.0f;
    values[fireous::kParamMeterGrComp] = 3.2f;
    values[fireous::kParamMeterGrLim] = 0.4f;
    values[fireous::kParamMeterInPeakL] = 0.42f;
    values[fireous::kParamMeterInPeakR] = 0.38f;
    values[fireous::kParamMeterOutPeakL] = 0.66f;
    values[fireous::kParamMeterOutPeakR] = 0.61f;
    values[fireous::kParamMeterLatency] = 5.0f;

    fireous::ui::Layout layout = fireous::ui::buildLayout(1280.0f, 900.0f, values, 1, theme);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1280, 900);
    cairo_t* cr = cairo_create(surface);

    fireous::ui::PaintState state;
    state.values = values;
    state.themeIndex = theme;
    state.selectedPeqBand = 1;
    state.sampleRate = 48000.0;
    state.presetName = "Drum Bus Glue";
    state.scaleLabel = "UI 100%";
    state.presetDeleteEnabled = true;

    fireous::ui::paint(cr, layout, state);

    cairo_surface_write_to_png(surface, outPath);
    std::printf("wrote %s\n", outPath);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return 0;
}
