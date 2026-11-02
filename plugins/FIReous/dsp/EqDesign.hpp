/*
 * FIReous - EQ magnitude target and linear-phase FIR design.
 *
 * The same RBJ response used by the IIR mode is sampled on a dense grid and
 * handed to the linear-phase FIR designer, so the IIR and LINEAR modes have
 * the same magnitude curve; only the phase differs.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Biquad.hpp"
#include "FirDesign.hpp"
#include "Fft.hpp"

namespace fireous {

static constexpr int kNumGraphicBands = 10;
static constexpr int kNumPeqBands = 6;

static constexpr float kGraphicFreqs[kNumGraphicBands] = {
    31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

struct EqBandSettings
{
    int type = 0;        // 0 = Bell, 1 = Low Shelf, 2 = High Shelf
    float freq = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
};

struct EqSettings
{
    bool eqOn = true;
    float eqGains[kNumGraphicBands] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    bool peqOn = true;
    EqBandSettings peq[kNumPeqBands];

    bool hpOn = false;
    float hpFreq = 20.0f;
    int hpSlope = 0;     // 0 = 12 dB/oct, 1 = 24 dB/oct

    bool lpOn = false;
    float lpFreq = 20000.0f;
    int lpSlope = 0;
};

inline void buildEqMagnitude(const EqSettings& s, double sampleRate, int K, std::vector<double>& mag)
{
    mag.assign((size_t)(K / 2 + 1), 1.0);

    Biquad graphic[kNumGraphicBands];
    if (s.eqOn)
    {
        for (int i = 0; i < kNumGraphicBands; ++i)
            graphic[i].setBell(sampleRate, kGraphicFreqs[i], s.eqGains[i], 1.4f);
    }

    Biquad peq[kNumPeqBands];
    if (s.peqOn)
    {
        for (int i = 0; i < kNumPeqBands; ++i)
        {
            const EqBandSettings& b = s.peq[i];
            const Biquad::Type type = (b.type == 1) ? Biquad::Type::LowShelf
                                   : (b.type == 2) ? Biquad::Type::HighShelf
                                                   : Biquad::Type::Bell;
            peq[i].set(type, sampleRate, b.freq, b.gainDb, b.q);
        }
    }

    Biquad hp1, hp2, lp1, lp2;
    const bool useHp = s.peqOn && s.hpOn;
    const bool useLp = s.peqOn && s.lpOn;
    if (useHp)
    {
        hp1.setHighpass(sampleRate, s.hpFreq, 0.7071f);
        if (s.hpSlope > 0)
            hp2.setHighpass(sampleRate, s.hpFreq, 0.7071f);
    }
    if (useLp)
    {
        lp1.setLowpass(sampleRate, s.lpFreq, 0.7071f);
        if (s.lpSlope > 0)
            lp2.setLowpass(sampleRate, s.lpFreq, 0.7071f);
    }

    for (int k = 0; k <= K / 2; ++k)
    {
        const double f = (double)k * sampleRate / (double)K;
        double m = 1.0;
        if (s.eqOn)
            for (int i = 0; i < kNumGraphicBands; ++i)
                m *= graphic[i].magnitudeAt(sampleRate, f);
        if (s.peqOn)
        {
            for (int i = 0; i < kNumPeqBands; ++i)
                m *= peq[i].magnitudeAt(sampleRate, f);
            if (useHp)
            {
                m *= hp1.magnitudeAt(sampleRate, f);
                if (s.hpSlope > 0)
                    m *= hp2.magnitudeAt(sampleRate, f);
            }
            if (useLp)
            {
                m *= lp1.magnitudeAt(sampleRate, f);
                if (s.lpSlope > 0)
                    m *= lp2.magnitudeAt(sampleRate, f);
            }
        }
        mag[(size_t)k] = std::max(0.0, m);
    }
}

inline std::vector<float> designEqFir(const EqSettings& s, int taps, double sampleRate)
{
    const int K = nextPowerOfTwo(std::max(2048, taps * 8));
    std::vector<double> mag;
    buildEqMagnitude(s, sampleRate, K, mag);
    return designLinearPhaseFromMagnitude(mag, taps, 8.6);
}

} // namespace fireous
