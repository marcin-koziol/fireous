/*
 * FIReous - stereo-linked safety limiter.
 *
 * lookahead 0/1/2 ms (optional). True-peak uses a simple 4x linear
 * inter-sample estimate; a proper oversampled detector can replace it later
 * without changing the interface.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace fireous {

class Limiter
{
public:
    static constexpr float kMaxLookaheadMs = 2.0f;

    void prepare(double sampleRate)
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const size_t maxSamples = (size_t)std::ceil(kMaxLookaheadMs * 0.001 * fSampleRate) + 4;
        fBufL.assign(maxSamples, 0.0f);
        fBufR.assign(maxSamples, 0.0f);
        fBufSize = maxSamples;
        reset();
    }

    void reset() noexcept
    {
        std::fill(fBufL.begin(), fBufL.end(), 0.0f);
        std::fill(fBufR.begin(), fBufR.end(), 0.0f);
        fWrite = 0;
        fGain = 1.0f;
        fPrevL = fPrevR = 0.0f;
        fGainReductionDb = 0.0f;
    }

    void setCeilingDb(float db) noexcept { fCeilingDb = std::clamp(db, -12.0f, 0.0f); }
    void setReleaseMs(float ms) noexcept { fReleaseMs = std::clamp(ms, 10.0f, 1000.0f); }
    void setLookaheadMs(float ms) noexcept
    {
        fLookaheadSamples = (int)std::lround(std::clamp(ms, 0.0f, kMaxLookaheadMs) * 0.001 * fSampleRate);
        fLookaheadSamples = std::min(fLookaheadSamples, (int)fBufSize - 2);
    }
    void setTruePeak(bool on) noexcept { fTruePeak = on; }

    void process(float& l, float& r) noexcept
    {
        const float peak = peakOf(l, r);
        fBufL[fWrite] = l;
        fBufR[fWrite] = r;

        const int look = fLookaheadSamples;
        float windowPeak = peak;
        for (int i = 1; i <= look; ++i)
        {
            const size_t idx = (fWrite + fBufSize - (size_t)i) % fBufSize;
            windowPeak = std::max(windowPeak, std::max(std::fabs(fBufL[idx]), std::fabs(fBufR[idx])));
        }

        const float ceiling = std::pow(10.0f, fCeilingDb / 20.0f);
        const float target = windowPeak > 1e-8f ? std::min(1.0f, ceiling / windowPeak) : 1.0f;
        if (target < fGain)
        {
            // Fast attack; the hard clip below catches the residual.
            const float a = 1.0f - std::exp(-1.0f / (0.0005f * (float)fSampleRate));
            fGain += a * (target - fGain);
        }
        else
        {
            const float a = 1.0f - std::exp(-1.0f / (fReleaseMs * 0.001f * (float)fSampleRate));
            fGain += a * (target - fGain);
        }

        const size_t readIdx = (fWrite + fBufSize - (size_t)look) % fBufSize;
        float outL = fBufL[readIdx] * fGain;
        float outR = fBufR[readIdx] * fGain;
        outL = std::clamp(outL, -ceiling, ceiling);
        outR = std::clamp(outR, -ceiling, ceiling);
        l = outL;
        r = outR;

        fGainReductionDb = -20.0f * std::log10(std::max(fGain, 1e-6f));
        fPrevL = fBufL[fWrite];
        fPrevR = fBufR[fWrite];
        fWrite = (fWrite + 1) % fBufSize;
    }

    float getGainReductionDb() const noexcept { return fGainReductionDb; }
    int getLookaheadSamples() const noexcept { return fLookaheadSamples; }

private:
    float peakOf(float l, float r) noexcept
    {
        float peak = std::max(std::fabs(l), std::fabs(r));
        if (fTruePeak)
        {
            // Cheap 4x inter-sample estimate; replace with a proper
            // oversampling filter when true-peak metering is implemented.
            for (int i = 1; i < 4; ++i)
            {
                const float t = (float)i * 0.25f;
                const float il = fPrevL + (l - fPrevL) * t;
                const float ir = fPrevR + (r - fPrevR) * t;
                peak = std::max(peak, std::max(std::fabs(il), std::fabs(ir)));
            }
        }
        return peak;
    }

    std::vector<float> fBufL, fBufR;
    size_t fBufSize = 0;
    size_t fWrite = 0;
    double fSampleRate = 44100.0;
    float fCeilingDb = -0.3f;
    float fReleaseMs = 100.0f;
    int fLookaheadSamples = 0;
    bool fTruePeak = false;
    float fGain = 1.0f;
    float fPrevL = 0.0f, fPrevR = 0.0f;
    float fGainReductionDb = 0.0f;
};

} // namespace fireous
