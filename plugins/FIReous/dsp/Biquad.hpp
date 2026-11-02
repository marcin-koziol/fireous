/*
 * FIReous - RBJ cookbook biquads. Used by the default IIR mode and as the
 * magnitude target source for the optional linear-phase FIR mode.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace fireous {

class Biquad
{
public:
    enum class Type { Bell = 0, LowShelf = 1, HighShelf = 2, Lowpass = 3, Highpass = 4 };

    void reset() noexcept { fZ1 = fZ2 = 0.0f; }

    void setIdentity() noexcept
    {
        fB0 = 1.0f; fB1 = fB2 = 0.0f;
        fA1 = fA2 = 0.0f;
        fZ1 = fZ2 = 0.0f;
    }

    void setBell(double sampleRate, float freq, float gainDb, float q) noexcept
    {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = omega(sampleRate, freq);
        const float alpha = alphaFor(w0, q);
        const float c = std::cos(w0);
        const float a0 = 1.0f + alpha / A;
        setNormalized(
            (1.0f + alpha * A) / a0,
            (-2.0f * c) / a0,
            (1.0f - alpha * A) / a0,
            (-2.0f * c) / a0,
            (1.0f - alpha / A) / a0);
    }

    void setLowShelf(double sampleRate, float freq, float gainDb, float q) noexcept
    {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = omega(sampleRate, freq);
        const float alpha = alphaFor(w0, q);
        const float c = std::cos(w0);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1.0f) + (A - 1.0f) * c + twoSqrtAAlpha;
        setNormalized(
            (A * ((A + 1.0f) - (A - 1.0f) * c + twoSqrtAAlpha)) / a0,
            (2.0f * A * ((A - 1.0f) - (A + 1.0f) * c)) / a0,
            (A * ((A + 1.0f) - (A - 1.0f) * c - twoSqrtAAlpha)) / a0,
            (-2.0f * ((A - 1.0f) + (A + 1.0f) * c)) / a0,
            ((A + 1.0f) + (A - 1.0f) * c - twoSqrtAAlpha) / a0);
    }

    void setHighShelf(double sampleRate, float freq, float gainDb, float q) noexcept
    {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = omega(sampleRate, freq);
        const float alpha = alphaFor(w0, q);
        const float c = std::cos(w0);
        const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
        const float a0 = (A + 1.0f) - (A - 1.0f) * c + twoSqrtAAlpha;
        setNormalized(
            (A * ((A + 1.0f) + (A - 1.0f) * c + twoSqrtAAlpha)) / a0,
            (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * c)) / a0,
            (A * ((A + 1.0f) + (A - 1.0f) * c - twoSqrtAAlpha)) / a0,
            (2.0f * ((A - 1.0f) - (A + 1.0f) * c)) / a0,
            ((A + 1.0f) - (A - 1.0f) * c - twoSqrtAAlpha) / a0);
    }

    void setLowpass(double sampleRate, float freq, float q) noexcept
    {
        const float w0 = omega(sampleRate, freq);
        const float alpha = alphaFor(w0, q);
        const float c = std::cos(w0);
        const float a0 = 1.0f + alpha;
        setNormalized(
            ((1.0f - c) * 0.5f) / a0,
            (1.0f - c) / a0,
            ((1.0f - c) * 0.5f) / a0,
            (-2.0f * c) / a0,
            (1.0f - alpha) / a0);
    }

    void setHighpass(double sampleRate, float freq, float q) noexcept
    {
        const float w0 = omega(sampleRate, freq);
        const float alpha = alphaFor(w0, q);
        const float c = std::cos(w0);
        const float a0 = 1.0f + alpha;
        setNormalized(
            ((1.0f + c) * 0.5f) / a0,
            (-(1.0f + c)) / a0,
            ((1.0f + c) * 0.5f) / a0,
            (-2.0f * c) / a0,
            (1.0f - alpha) / a0);
    }

    void set(Type type, double sampleRate, float freq, float gainDb, float q) noexcept
    {
        switch (type)
        {
        case Type::LowShelf:  setLowShelf(sampleRate, freq, gainDb, q); break;
        case Type::HighShelf: setHighShelf(sampleRate, freq, gainDb, q); break;
        case Type::Lowpass:   setLowpass(sampleRate, freq, q); break;
        case Type::Highpass:  setHighpass(sampleRate, freq, q); break;
        case Type::Bell:
        default:              setBell(sampleRate, freq, gainDb, q); break;
        }
    }

    float process(float x) noexcept
    {
        const float y = fB0 * x + fZ1;
        fZ1 = fB1 * x - fA1 * y + fZ2;
        fZ2 = fB2 * x - fA2 * y;
        return y;
    }

    float magnitudeAt(double sampleRate, double freq) const noexcept
    {
        const double w = 2.0 * M_PI * std::clamp(freq, 1.0, sampleRate * 0.499) / sampleRate;
        const std::complex<double> z1 = std::polar(1.0, -w);
        const std::complex<double> z2 = z1 * z1;
        const std::complex<double> num = (double)fB0 + (double)fB1 * z1 + (double)fB2 * z2;
        const std::complex<double> den = 1.0 + (double)fA1 * z1 + (double)fA2 * z2;
        return (float)std::abs(num / den);
    }

private:
    static float omega(double sampleRate, float freq) noexcept
    {
        const double fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        const double f = std::clamp((double)freq, 10.0, fs * 0.49);
        return (float)(2.0 * M_PI * f / fs);
    }

    static float alphaFor(float w0, float q) noexcept
    {
        const float qq = std::clamp(q, 0.05f, 20.0f);
        return std::sin(w0) / (2.0f * qq);
    }

    void setNormalized(float b0, float b1, float b2, float a1, float a2) noexcept
    {
        fB0 = b0; fB1 = b1; fB2 = b2;
        fA1 = a1; fA2 = a2;
        // Keep the state bounded across coefficient changes.
        fZ1 = std::clamp(fZ1, -16.0f, 16.0f);
        fZ2 = std::clamp(fZ2, -16.0f, 16.0f);
    }

    float fB0 = 1.0f, fB1 = 0.0f, fB2 = 0.0f;
    float fA1 = 0.0f, fA2 = 0.0f;
    float fZ1 = 0.0f, fZ2 = 0.0f;
};

} // namespace fireous
