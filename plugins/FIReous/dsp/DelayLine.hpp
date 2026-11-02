/*
 * FIReous - stereo delay effect plus independent per-channel alignment.
 *
 * The effect is a conventional feedback delay with optional ping-pong and a
 * one-pole low-pass in the feedback path. The alignment stage is a separate,
 * short fractional delay (0..10 ms) per channel; it is intentional user
 * delay, not reported plugin latency.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace fireous {

class StereoDelay
{
public:
    static constexpr float kMaxSeconds = 2.0f;

    void prepare(double sampleRate)
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const size_t size = (size_t)std::ceil(kMaxSeconds * fSampleRate) + 4;
        fBufL.assign(size, 0.0f);
        fBufR.assign(size, 0.0f);
        fSize = size;
        fWrite = 0;
        reset();
    }

    void reset() noexcept
    {
        std::fill(fBufL.begin(), fBufL.end(), 0.0f);
        std::fill(fBufR.begin(), fBufR.end(), 0.0f);
        fWrite = 0;
        fToneL = fToneR = 0.0f;
    }

    void setTimeSeconds(float seconds) noexcept
    {
        const float clamped = std::clamp(seconds, 0.001f, kMaxSeconds);
        fTargetDelay = clamped * (float)fSampleRate;
    }

    void setFeedback(float fb) noexcept { fFeedback = std::clamp(fb, 0.0f, 0.95f); }
    void setPingPong(float pp) noexcept { fPingPong = std::clamp(pp, 0.0f, 1.0f); }
    void setMix(float mix) noexcept { fMix = std::clamp(mix, 0.0f, 1.0f); }
    void setTone(float tone) noexcept { fTone = std::clamp(tone, 0.0f, 1.0f); }

    void process(float inL, float inR, float& outL, float& outR) noexcept
    {
        if (fBufL.empty() || fBufR.empty())
        {
            outL = inL;
            outR = inR;
            return;
        }

        // Smoothed delay time, so dragging the time knob doesn't jump.
        fCurrentDelay += (fTargetDelay - fCurrentDelay) * 0.0005f;
        const float delay = std::clamp(fCurrentDelay, 1.0f, (float)fSize - 3.0f);

        const float wetL = read(fBufL, (float)fWrite - delay);
        const float wetR = read(fBufR, (float)fWrite - delay);

        const float crossL = wetL + (wetR - wetL) * fPingPong;
        const float crossR = wetR + (wetL - wetR) * fPingPong;

        // One-pole low-pass in the feedback path.
        const float cutoff = 20000.0f * std::pow(0.02f, fTone);
        const float a = 1.0f - std::exp(-2.0f * (float)M_PI * cutoff / (float)fSampleRate);
        fToneL += a * (crossL - fToneL);
        fToneR += a * (crossR - fToneR);

        fBufL[fWrite] = inL + fToneL * fFeedback;
        fBufR[fWrite] = inR + fToneR * fFeedback;
        fWrite = (fWrite + 1) % fSize;

        outL = inL + (wetL - inL) * fMix;
        outR = inR + (wetR - inR) * fMix;
    }

private:
    float read(const std::vector<float>& buf, float pos) const noexcept
    {
        const float size = (float)fSize;
        while (pos < 0.0f) pos += size;
        while (pos >= size) pos -= size;
        const int i0 = (int)pos;
        const int i1 = (i0 + 1) % (int)fSize;
        const float frac = pos - (float)i0;
        return buf[(size_t)i0] * (1.0f - frac) + buf[(size_t)i1] * frac;
    }

    std::vector<float> fBufL, fBufR;
    size_t fSize = 0;
    size_t fWrite = 0;
    double fSampleRate = 44100.0;
    float fTargetDelay = 11025.0f;
    float fCurrentDelay = 11025.0f;
    float fFeedback = 0.35f;
    float fPingPong = 0.0f;
    float fMix = 0.25f;
    float fTone = 0.0f;
    float fToneL = 0.0f, fToneR = 0.0f;
};

class StereoAlign
{
public:
    static constexpr float kMaxSeconds = 0.060f;

    void prepare(double sampleRate)
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        const size_t size = (size_t)std::ceil(kMaxSeconds * fSampleRate) + 4;
        fBufL.assign(size, 0.0f);
        fBufR.assign(size, 0.0f);
        fSize = size;
        fWrite = 0;
        reset();
    }

    void reset() noexcept
    {
        std::fill(fBufL.begin(), fBufL.end(), 0.0f);
        std::fill(fBufR.begin(), fBufR.end(), 0.0f);
        fWrite = 0;
    }

    void setDelaysMs(float leftMs, float rightMs) noexcept
    {
        fDelayL = std::clamp(leftMs, 0.0f, 60.0f) * 0.001f * (float)fSampleRate;
        fDelayR = std::clamp(rightMs, 0.0f, 60.0f) * 0.001f * (float)fSampleRate;
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept
    {
        if (fBufL.empty() || fBufR.empty())
        {
            outL = inL;
            outR = inR;
            return;
        }
        fBufL[fWrite] = inL;
        fBufR[fWrite] = inR;
        outL = read(fBufL, (float)fWrite - fDelayL);
        outR = read(fBufR, (float)fWrite - fDelayR);
        fWrite = (fWrite + 1) % fSize;
    }

private:
    float read(const std::vector<float>& buf, float pos) const noexcept
    {
        const float size = (float)fSize;
        while (pos < 0.0f) pos += size;
        while (pos >= size) pos -= size;
        const int i0 = (int)pos;
        const int i1 = (i0 + 1) % (int)fSize;
        const float frac = pos - (float)i0;
        return buf[(size_t)i0] * (1.0f - frac) + buf[(size_t)i1] * frac;
    }

    std::vector<float> fBufL, fBufR;
    size_t fSize = 0;
    size_t fWrite = 0;
    double fSampleRate = 44100.0;
    float fDelayL = 0.0f, fDelayR = 0.0f;
};

} // namespace fireous
