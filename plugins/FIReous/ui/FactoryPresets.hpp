/*
 * FIReous - factory presets, compiled in and seeded into the user's preset
 * directory on first run (same pattern as the rest of the family).
 */

#pragma once

#include "PresetStore.hpp"

#include <cstring>
#include <utility>
#include <vector>

namespace fireous {
namespace ui {

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> overrides;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Flat / Null", {
            { "phase_mode", 0.0f }, { "lim_on", 0.0f },
        }},
        { "Vocal Air", {
            { "phase_mode", 0.0f },
            { "hp_on", 1.0f }, { "hp_freq", 80.0f },
            { "peq6_gain", 3.5f }, { "peq6_freq", 12000.0f }, { "peq6_q", 0.7f },
            { "comp_on", 1.0f }, { "comp_threshold", -18.0f }, { "comp_ratio", 2.0f },
            { "comp_attack", 15.0f }, { "comp_release", 180.0f }, { "comp_makeup", 2.0f },
        }},
        { "Drum Bus Glue", {
            { "phase_mode", 0.0f },
            { "eq_gain_63", 1.5f }, { "eq_gain_125", 1.0f },
            { "comp_on", 1.0f }, { "comp_threshold", -16.0f }, { "comp_ratio", 2.5f },
            { "comp_attack", 20.0f }, { "comp_release", 150.0f }, { "comp_knee", 8.0f },
            { "comp_makeup", 2.0f }, { "lim_on", 1.0f }, { "lim_ceiling", -0.5f },
        }},
        { "Bass Tight", {
            { "phase_mode", 0.0f },
            { "hp_on", 1.0f }, { "hp_freq", 28.0f },
            { "peq1_gain", 2.0f }, { "peq1_freq", 60.0f },
            { "peq2_gain", -3.0f }, { "peq2_freq", 220.0f }, { "peq2_q", 1.2f },
            { "comp_on", 1.0f }, { "comp_threshold", -22.0f }, { "comp_ratio", 3.0f },
            { "comp_attack", 8.0f }, { "comp_release", 120.0f },
        }},
        { "Bright Master", {
            { "phase_mode", 1.0f }, { "linear_latency", 2.0f },
            { "peq6_gain", 2.5f }, { "peq6_freq", 11000.0f }, { "peq6_q", 0.7f },
            { "lim_on", 1.0f }, { "lim_ceiling", -0.3f }, { "lim_lookahead", 1.0f },
        }},
        { "Delay Space", {
            { "delay_on", 1.0f }, { "delay_mode", 0.0f },
            { "delay_sync", 9.0f }, { "delay_feedback", 0.42f },
            { "delay_pingpong", 0.6f }, { "delay_mix", 0.30f }, { "delay_tone", 0.25f },
        }},
        { "Phase Check", {
            { "phase_mode", 1.0f }, { "linear_latency", 2.0f },
            { "lim_on", 0.0f }, { "comp_on", 0.0f }, { "delay_on", 0.0f },
        }},
    };
    return presets;
}

inline void seedFactoryPresets()
{
    for (const FactoryPreset& fp : factoryPresets())
    {
        float values[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            values[i] = getParamInfo(i).def;

        for (const auto& kv : fp.overrides)
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
            {
                if (std::strcmp(getParamInfo(i).symbol, kv.first) == 0)
                {
                    values[i] = kv.second;
                    break;
                }
            }
        }

        savePreset(fp.name, values);
    }
}

} // namespace ui
} // namespace fireous
