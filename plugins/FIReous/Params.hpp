/*
 * FIReous - shared parameter table for the DSP and (later) the UI.
 *
 * v0.1.0 scope: IIR graphic EQ + parametric EQ + HP/LP, stereo delay with
 * per-channel alignment, global compressor, global limiter, and an optional
 * linear-phase FIR mode for the EQ/PEQ block.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fireous {

enum Params : uint32_t {
    kParamInputTrim = 0,
    kParamOutputGain,
    kParamBypass,
    kParamPhaseMode,          // 0 = IIR, 1 = LINEAR
    kParamLinearLatency,      // 0=2.7, 1=5, 2=10, 3=20, 4=40 ms
    kParamMonitor,            // 0 = Normal, 1 = Delta

    kParamEqOn,
    kParamEqGain31,
    kParamEqGain63,
    kParamEqGain125,
    kParamEqGain250,
    kParamEqGain500,
    kParamEqGain1k,
    kParamEqGain2k,
    kParamEqGain4k,
    kParamEqGain8k,
    kParamEqGain16k,

    kParamPeqOn,
    kParamPeqType1,
    kParamPeqType2,
    kParamPeqType3,
    kParamPeqType4,
    kParamPeqType5,
    kParamPeqType6,
    kParamPeqFreq1,
    kParamPeqFreq2,
    kParamPeqFreq3,
    kParamPeqFreq4,
    kParamPeqFreq5,
    kParamPeqFreq6,
    kParamPeqGain1,
    kParamPeqGain2,
    kParamPeqGain3,
    kParamPeqGain4,
    kParamPeqGain5,
    kParamPeqGain6,
    kParamPeqQ1,
    kParamPeqQ2,
    kParamPeqQ3,
    kParamPeqQ4,
    kParamPeqQ5,
    kParamPeqQ6,
    kParamHpOn,
    kParamHpFreq,
    kParamHpSlope,
    kParamLpOn,
    kParamLpFreq,
    kParamLpSlope,

    kParamDelayOn,
    kParamDelayMode,          // 0 = Effect, 1 = Align, 2 = Both
    kParamDelaySync,
    kParamDelayTimeMs,
    kParamDelayFeedback,
    kParamDelayPingPong,
    kParamDelayMix,
    kParamDelayTone,
    kParamAlignLMs,
    kParamAlignRMs,

    kParamCompOn,
    kParamCompThreshold,
    kParamCompRatio,
    kParamCompAttack,
    kParamCompRelease,
    kParamCompKnee,
    kParamCompMakeup,
    kParamCompDetector,       // 0 = RMS, 1 = Peak
    kParamCompLink,
    kParamCompAutoMakeup,
    kParamCompKey,            // 0 = Internal, 1 = External sidechain

    kParamLimOn,
    kParamLimCeiling,
    kParamLimRelease,
    kParamLimLookahead,       // 0 / 1 / 2 ms
    kParamLimTruePeak,

    kParamChainPosEq,
    kParamChainPosPeq,
    kParamChainPosDelay,
    kParamChainPosComp,
    kParamChainPosLim,

    kParamFirstActive,

    // output-only meters (must stay after kParamFirstActive)
    kParamMeterInPeakL = kParamFirstActive,
    kParamMeterInPeakR,
    kParamMeterOutPeakL,
    kParamMeterOutPeakR,
    kParamMeterGrComp,
    kParamMeterGrLim,
    kParamMeterLatency,
    kParamMeterPhaseMode,

    kParamCount
};

enum class ParamShape { Linear, Logarithmic };

struct ParamInfo
{
    const char* name;
    const char* symbol;
    const char* unit;
    float min;
    float max;
    float def;
    ParamShape shape;
    bool stepped;
};

inline const ParamInfo& getParamInfo(uint32_t index) noexcept
{
    static constexpr ParamInfo table[kParamCount] = {
        { "Input Trim", "input_trim", "dB", -24.0f, 24.0f, 0.0f, ParamShape::Linear, false },
        { "Output Gain", "output_gain", "dB", -24.0f, 24.0f, 0.0f, ParamShape::Linear, false },
        { "Bypass", "bypass", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "Phase Mode", "phase_mode", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "Linear Latency", "linear_latency", "", 0.0f, 4.0f, 2.0f, ParamShape::Linear, true },
        { "Monitor", "monitor", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },

        { "EQ On", "eq_on", "", 0.0f, 1.0f, 1.0f, ParamShape::Linear, true },
        { "EQ 31 Hz", "eq_gain_31", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 63 Hz", "eq_gain_63", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 125 Hz", "eq_gain_125", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 250 Hz", "eq_gain_250", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 500 Hz", "eq_gain_500", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 1 kHz", "eq_gain_1k", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 2 kHz", "eq_gain_2k", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 4 kHz", "eq_gain_4k", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 8 kHz", "eq_gain_8k", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },
        { "EQ 16 kHz", "eq_gain_16k", "dB", -12.0f, 12.0f, 0.0f, ParamShape::Linear, false },

        { "PEQ On", "peq_on", "", 0.0f, 1.0f, 1.0f, ParamShape::Linear, true },
        { "PEQ 1 Type", "peq1_type", "", 0.0f, 2.0f, 1.0f, ParamShape::Linear, true },
        { "PEQ 2 Type", "peq2_type", "", 0.0f, 2.0f, 0.0f, ParamShape::Linear, true },
        { "PEQ 3 Type", "peq3_type", "", 0.0f, 2.0f, 0.0f, ParamShape::Linear, true },
        { "PEQ 4 Type", "peq4_type", "", 0.0f, 2.0f, 0.0f, ParamShape::Linear, true },
        { "PEQ 5 Type", "peq5_type", "", 0.0f, 2.0f, 0.0f, ParamShape::Linear, true },
        { "PEQ 6 Type", "peq6_type", "", 0.0f, 2.0f, 2.0f, ParamShape::Linear, true },
        { "PEQ 1 Freq", "peq1_freq", "Hz", 20.0f, 20000.0f, 80.0f, ParamShape::Logarithmic, false },
        { "PEQ 2 Freq", "peq2_freq", "Hz", 20.0f, 20000.0f, 250.0f, ParamShape::Logarithmic, false },
        { "PEQ 3 Freq", "peq3_freq", "Hz", 20.0f, 20000.0f, 800.0f, ParamShape::Logarithmic, false },
        { "PEQ 4 Freq", "peq4_freq", "Hz", 20.0f, 20000.0f, 2500.0f, ParamShape::Logarithmic, false },
        { "PEQ 5 Freq", "peq5_freq", "Hz", 20.0f, 20000.0f, 6000.0f, ParamShape::Logarithmic, false },
        { "PEQ 6 Freq", "peq6_freq", "Hz", 20.0f, 20000.0f, 12000.0f, ParamShape::Logarithmic, false },
        { "PEQ 1 Gain", "peq1_gain", "dB", -18.0f, 18.0f, 0.0f, ParamShape::Linear, false },
        { "PEQ 2 Gain", "peq2_gain", "dB", -18.0f, 18.0f, 0.0f, ParamShape::Linear, false },
        { "PEQ 3 Gain", "peq3_gain", "dB", -18.0f, 18.0f, 0.0f, ParamShape::Linear, false },
        { "PEQ 4 Gain", "peq4_gain", "dB", -18.0f, 18.0f, 0.0f, ParamShape::Linear, false },
        { "PEQ 5 Gain", "peq5_gain", "dB", -18.0f, 18.0f, 0.0f, ParamShape::Linear, false },
        { "PEQ 6 Gain", "peq6_gain", "dB", -18.0f, 18.0f, 0.0f, ParamShape::Linear, false },
        { "PEQ 1 Q", "peq1_q", "", 0.1f, 10.0f, 0.7f, ParamShape::Logarithmic, false },
        { "PEQ 2 Q", "peq2_q", "", 0.1f, 10.0f, 1.0f, ParamShape::Logarithmic, false },
        { "PEQ 3 Q", "peq3_q", "", 0.1f, 10.0f, 1.0f, ParamShape::Logarithmic, false },
        { "PEQ 4 Q", "peq4_q", "", 0.1f, 10.0f, 1.0f, ParamShape::Logarithmic, false },
        { "PEQ 5 Q", "peq5_q", "", 0.1f, 10.0f, 1.0f, ParamShape::Logarithmic, false },
        { "PEQ 6 Q", "peq6_q", "", 0.1f, 10.0f, 0.7f, ParamShape::Logarithmic, false },
        { "HP On", "hp_on", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "HP Freq", "hp_freq", "Hz", 20.0f, 2000.0f, 20.0f, ParamShape::Logarithmic, false },
        { "HP Slope", "hp_slope", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "LP On", "lp_on", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "LP Freq", "lp_freq", "Hz", 1000.0f, 20000.0f, 20000.0f, ParamShape::Logarithmic, false },
        { "LP Slope", "lp_slope", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },

        { "Delay On", "delay_on", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "Delay Mode", "delay_mode", "", 0.0f, 2.0f, 0.0f, ParamShape::Linear, true },
        { "Delay Sync", "delay_sync", "", 0.0f, 16.0f, 0.0f, ParamShape::Linear, true },
        { "Delay Time", "delay_time_ms", "ms", 1.0f, 2000.0f, 250.0f, ParamShape::Logarithmic, false },
        { "Delay Feedback", "delay_feedback", "", 0.0f, 0.95f, 0.35f, ParamShape::Linear, false },
        { "Delay Ping-Pong", "delay_pingpong", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
        { "Delay Mix", "delay_mix", "", 0.0f, 1.0f, 0.25f, ParamShape::Linear, false },
        { "Delay Tone", "delay_tone", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
        { "Align L", "align_l_ms", "ms", 0.0f, 10.0f, 0.0f, ParamShape::Linear, false },
        { "Align R", "align_r_ms", "ms", 0.0f, 10.0f, 0.0f, ParamShape::Linear, false },

        { "Comp On", "comp_on", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "Comp Threshold", "comp_threshold", "dB", -60.0f, 0.0f, -12.0f, ParamShape::Linear, false },
        { "Comp Ratio", "comp_ratio", "", 1.0f, 20.0f, 2.0f, ParamShape::Linear, false },
        { "Comp Attack", "comp_attack", "ms", 0.1f, 100.0f, 10.0f, ParamShape::Logarithmic, false },
        { "Comp Release", "comp_release", "ms", 10.0f, 1000.0f, 150.0f, ParamShape::Logarithmic, false },
        { "Comp Knee", "comp_knee", "dB", 0.0f, 24.0f, 6.0f, ParamShape::Linear, false },
        { "Comp Makeup", "comp_makeup", "dB", 0.0f, 24.0f, 0.0f, ParamShape::Linear, false },
        { "Comp Detector", "comp_detector", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "Comp Link", "comp_link", "", 0.0f, 1.0f, 1.0f, ParamShape::Linear, false },
        { "Comp Auto Makeup", "comp_auto_makeup", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },
        { "Comp Key", "comp_key", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },

        { "Limiter On", "lim_on", "", 0.0f, 1.0f, 1.0f, ParamShape::Linear, true },
        { "Limiter Ceiling", "lim_ceiling", "dB", -12.0f, 0.0f, -0.3f, ParamShape::Linear, false },
        { "Limiter Release", "lim_release", "ms", 10.0f, 1000.0f, 100.0f, ParamShape::Logarithmic, false },
        { "Limiter Lookahead", "lim_lookahead", "ms", 0.0f, 2.0f, 0.0f, ParamShape::Linear, true },
        { "Limiter True Peak", "lim_true_peak", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, true },

        { "Chain: EQ", "chain_pos_eq", "", 0.0f, 4.0f, 0.0f, ParamShape::Linear, true },
        { "Chain: PEQ", "chain_pos_peq", "", 0.0f, 4.0f, 1.0f, ParamShape::Linear, true },
        { "Chain: Delay", "chain_pos_delay", "", 0.0f, 4.0f, 2.0f, ParamShape::Linear, true },
        { "Chain: Compressor", "chain_pos_comp", "", 0.0f, 4.0f, 3.0f, ParamShape::Linear, true },
        { "Chain: Limiter", "chain_pos_lim", "", 0.0f, 4.0f, 4.0f, ParamShape::Linear, true },

        // meters
        { "Meter In Peak L", "meter_in_peak_l", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
        { "Meter In Peak R", "meter_in_peak_r", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
        { "Meter Out Peak L", "meter_out_peak_l", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
        { "Meter Out Peak R", "meter_out_peak_r", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
        { "Meter GR Comp", "meter_gr_comp", "dB", 0.0f, 40.0f, 0.0f, ParamShape::Linear, false },
        { "Meter GR Limiter", "meter_gr_lim", "dB", 0.0f, 40.0f, 0.0f, ParamShape::Linear, false },
        { "Meter Latency", "meter_latency", "", 0.0f, 200000.0f, 0.0f, ParamShape::Linear, false },
        { "Meter Phase Mode", "meter_phase_mode", "", 0.0f, 1.0f, 0.0f, ParamShape::Linear, false },
    };
    return table[index];
}

inline bool isOutputParam(uint32_t index) noexcept
{
    return index >= kParamFirstActive && index < kParamCount;
}

inline bool isSteppedParam(uint32_t index) noexcept
{
    return index < kParamCount && getParamInfo(index).stepped;
}

inline bool isHostPosition(uint32_t index) noexcept
{
    return index < kParamCount && getParamInfo(index).shape == ParamShape::Logarithmic;
}

inline float paramToHost(uint32_t index, float value) noexcept
{
    if (!isHostPosition(index))
        return value;
    const ParamInfo& info = getParamInfo(index);
    const float lo = std::max(info.min, 1e-6f);
    const float v = std::clamp(value, lo, info.max);
    return 100.0f * std::log(v / lo) / std::log(info.max / lo);
}

inline float paramFromHost(uint32_t index, float value) noexcept
{
    if (!isHostPosition(index))
        return value;
    const ParamInfo& info = getParamInfo(index);
    const float lo = std::max(info.min, 1e-6f);
    return lo * std::pow(info.max / lo, std::clamp(value, 0.0f, 100.0f) * 0.01f);
}

// -----------------------------------------------------------------------------------------------------------
// chain order

enum StageKind { kStageEq = 0, kStagePeq, kStageDelay, kStageComp, kStageLim, kStageCount };

static constexpr uint32_t kChainParamForStage[kStageCount] = {
    kParamChainPosEq, kParamChainPosPeq, kParamChainPosDelay, kParamChainPosComp, kParamChainPosLim
};

// chainPos is a compact 5-element array in StageKind order:
// [EQ, PEQ, Delay, Compressor, Limiter]. The UI builds it from the full
// parameter array with kChainParamForStage[].
inline void resolveChainOrder(const float* chainPos, int order[kStageCount]) noexcept
{
    struct Ranked { float pos; int stage; };
    Ranked ranked[kStageCount] = {
        { chainPos[kStageEq],    kStageEq },
        { chainPos[kStagePeq],   kStagePeq },
        { chainPos[kStageDelay], kStageDelay },
        { chainPos[kStageComp],  kStageComp },
        { chainPos[kStageLim],   kStageLim },
    };
    for (int i = 1; i < kStageCount; ++i)
    {
        Ranked key = ranked[i];
        int j = i - 1;
        while (j >= 0 && ranked[j].pos > key.pos)
        {
            ranked[j + 1] = ranked[j];
            --j;
        }
        ranked[j + 1] = key;
    }
    for (int i = 0; i < kStageCount; ++i)
        order[i] = ranked[i].stage;
}

inline float linearLatencyMsFromIndex(int index) noexcept
{
    static constexpr float values[5] = { 2.7f, 5.0f, 10.0f, 20.0f, 40.0f };
    return values[std::clamp(index, 0, 4)];
}

} // namespace fireous
