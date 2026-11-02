/*
 * FIReous - linear-phase FIR design from a real magnitude target.
 *
 * The target is sampled on a dense frequency grid, given linear phase
 * exp(-j*w*M), transformed with an IFFT, windowed with a Kaiser window and
 * normalized to the strongest target bin.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

#include "Fft.hpp"

namespace fireous {

inline double besselI0(double x) noexcept
{
    double sum = 1.0;
    double term = 1.0;
    const double half = x * 0.5;
    for (int k = 1; k < 64; ++k)
    {
        term *= (half / (double)k) * (half / (double)k);
        sum += term;
        if (term < 1e-14 * sum)
            break;
    }
    return sum;
}

inline double kaiserWindow(int n, int taps, double beta) noexcept
{
    if (taps <= 1)
        return 1.0;
    const double center = (double)(taps - 1) * 0.5;
    const double r = ((double)n - center) / center;
    const double arg = 1.0 - r * r;
    if (arg <= 0.0)
        return 0.0;
    return besselI0(beta * std::sqrt(arg)) / besselI0(beta);
}

inline double firMagnitudeAtBin(const std::vector<float>& h, int k, int K) noexcept
{
    double re = 0.0, im = 0.0;
    for (size_t n = 0; n < h.size(); ++n)
    {
        const double w = -2.0 * M_PI * (double)k * (double)n / (double)K;
        re += (double)h[n] * std::cos(w);
        im += (double)h[n] * std::sin(w);
    }
    return std::sqrt(re * re + im * im);
}

// mag has K/2+1 bins (0..Nyquist); taps must be odd and <= K.
inline std::vector<float> designLinearPhaseFromMagnitude(const std::vector<double>& mag,
                                                          int taps,
                                                          double beta = 8.6)
{
    const int K = std::max(2, ((int)mag.size() - 1) * 2);
    const int M = (taps - 1) / 2;
    std::vector<std::complex<double>> H((size_t)K, {0.0, 0.0});

    for (int k = 0; k <= K / 2; ++k)
    {
        const double m = (k < (int)mag.size()) ? mag[(size_t)k] : 0.0;
        const double phase = -2.0 * M_PI * (double)k * (double)M / (double)K;
        H[(size_t)k] = std::polar(std::max(0.0, m), phase);
    }
    for (int k = K / 2 + 1; k < K; ++k)
        H[(size_t)k] = std::conj(H[(size_t)(K - k)]);

    fft(H, true);

    std::vector<float> h((size_t)taps, 0.0f);
    for (int n = 0; n < taps; ++n)
        h[(size_t)n] = (float)(H[(size_t)n].real() * kaiserWindow(n, taps, beta));

    // Normalize against the strongest target bin.
    int kmax = 0;
    for (int k = 1; k <= K / 2; ++k)
        if (mag[(size_t)k] > mag[(size_t)kmax])
            kmax = k;
    const double target = mag[(size_t)kmax];
    if (target > 1e-9)
    {
        const double actual = firMagnitudeAtBin(h, kmax, K);
        if (actual > 1e-12)
        {
            const float scale = (float)(target / actual);
            for (auto& x : h)
                x *= scale;
        }
    }

    return h;
}

inline std::vector<float> designLowpass(double fc, int taps, double sampleRate, double beta = 8.6)
{
    const int M = (taps - 1) / 2;
    const double ft = std::clamp(fc / sampleRate, 1e-5, 0.499);
    std::vector<float> h((size_t)taps, 0.0f);
    double sum = 0.0;
    for (int n = 0; n < taps; ++n)
    {
        const double x = (double)(n - M);
        const double sinc = (std::fabs(x) < 1e-12) ? 1.0 : std::sin(2.0 * M_PI * ft * x) / (M_PI * x);
        h[(size_t)n] = (float)(2.0 * ft * sinc * kaiserWindow(n, taps, beta));
        sum += h[(size_t)n];
    }
    if (std::fabs(sum) > 1e-12)
        for (auto& x : h)
            x = (float)(x / sum);
    return h;
}

inline int tapsForLatencyMs(float ms, double sampleRate) noexcept
{
    const int taps = 2 * (int)std::lround(std::max(0.1f, ms) * 0.001f * (float)sampleRate) + 1;
    return std::clamp(taps, 65, 16385);
}

} // namespace fireous
