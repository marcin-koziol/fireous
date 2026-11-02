/*
 * FIReous - stereo-linked feed-forward compressor.
 *
 * Detector: RMS or peak. Link blends between independent L/R detectors
 * (0%) and a fully linked detector (100%). No lookahead, so it adds no
 * reported latency.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace fireous {

class Compressor
{
public:
    void prepare(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fRmsL = fRmsR = 0.0f;
        fGainDbL = fGainDbR = 0.0f;
    }

    void reset() noexcept
    {
        fRmsL = fRmsR = 0.0f;
        fGainDbL = fGainDbR = 0.0f;
    }

    void setThreshold(float db) noexcept { fThreshold = std::clamp(db, -60.0f, 0.0f); }
    void setRatio(float ratio) noexcept { fRatio = std::clamp(ratio, 1.0f, 20.0f); }
    void setAttackMs(float ms) noexcept { fAttackMs = std::clamp(ms, 0.1f, 100.0f); }
    void setReleaseMs(float ms) noexcept { fReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }
    void setKnee(float db) noexcept { fKnee = std::clamp(db, 0.0f, 24.0f); }
    void setMakeup(float db) noexcept { fMakeupDb = std::clamp(db, 0.0f, 24.0f); }
    void setDetector(bool peak) noexcept { fPeak = peak; }
    void setLink(float link01) noexcept { fLink = std::clamp(link01, 0.0f, 1.0f); }
    void setAutoMakeup(bool on) noexcept { fAutoMakeup = on; }

    void process(float& l, float& r, float keyL, float keyR) noexcept
    {
        const float levelL = detect(keyL, fRmsL);
        const float levelR = detect(keyR, fRmsR);
        const float common = std::max(levelL, levelR);
        const float detL = levelL + (common - levelL) * fLink;
        const float detR = levelR + (common - levelR) * fLink;

        const float grL = gainReductionDb(detL);
        const float grR = gainReductionDb(detR);

        fGainDbL = smooth(fGainDbL, grL, fAttackMs, fReleaseMs);
        fGainDbR = smooth(fGainDbR, grR, fAttackMs, fReleaseMs);

        const float makeup = fMakeupDb + (fAutoMakeup ? autoMakeupDb() : 0.0f);
        const float gL = std::pow(10.0f, (fGainDbL + makeup) / 20.0f);
        const float gR = std::pow(10.0f, (fGainDbR + makeup) / 20.0f);
        l *= gL;
        r *= gR;

        fGainReductionDb = std::max(-fGainDbL, -fGainDbR);
    }

    float getGainReductionDb() const noexcept { return fGainReductionDb; }

private:
    float detect(float x, float& rmsState) noexcept
    {
        const float level = std::fabs(x);
        if (fPeak)
            return level;
        const float a = 1.0f - std::exp(-1.0f / (0.010f * (float)fSampleRate));
        rmsState += a * (level * level - rmsState);
        return std::sqrt(std::max(0.0f, rmsState));
    }

    float gainReductionDb(float level) const noexcept
    {
        const float x = 20.0f * std::log10(std::max(level, 1e-9f));
        if (x <= fThreshold - fKnee * 0.5f)
            return 0.0f;
        if (fKnee > 0.0f && x < fThreshold + fKnee * 0.5f)
        {
            const float d = x - fThreshold + fKnee * 0.5f;
            return (1.0f / fRatio - 1.0f) * d * d / (2.0f * fKnee);
        }
        return (x - fThreshold) * (1.0f / fRatio - 1.0f);
    }

    float smooth(float current, float target, float attackMs, float releaseMs) const noexcept
    {
        const bool attacking = target < current;
        const float ms = attacking ? attackMs : releaseMs;
        const float a = 1.0f - std::exp(-1.0f / (ms * 0.001f * (float)fSampleRate));
        return current + a * (target - current);
    }

    float autoMakeupDb() const noexcept
    {
        // Common make-up approximation for a downward compressor.
        return std::clamp(-fThreshold * (1.0f - 1.0f / fRatio), 0.0f, 24.0f);
    }

    double fSampleRate = 44100.0;
    float fThreshold = -12.0f;
    float fRatio = 2.0f;
    float fAttackMs = 10.0f;
    float fReleaseMs = 150.0f;
    float fKnee = 6.0f;
    float fMakeupDb = 0.0f;
    float fLink = 1.0f;
    bool fPeak = false;
    bool fAutoMakeup = false;
    float fRmsL = 0.0f, fRmsR = 0.0f;
    float fGainDbL = 0.0f, fGainDbR = 0.0f;
    float fGainReductionDb = 0.0f;
};

} // namespace fireous
