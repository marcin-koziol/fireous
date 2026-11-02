/*
 * FIReous - small radix-2 complex FFT used only for FIR coefficient design
 * (never on the audio thread).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace fireous {

inline int nextPowerOfTwo(int n) noexcept
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

inline void fft(std::vector<std::complex<double>>& a, bool inverse) noexcept
{
    const int n = (int)a.size();
    if (n <= 1)
        return;

    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[(size_t)i], a[(size_t)j]);
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * M_PI / (double)len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j)
            {
                const std::complex<double> u = a[(size_t)(i + j)];
                const std::complex<double> v = a[(size_t)(i + j + half)] * w;
                a[(size_t)(i + j)] = u + v;
                a[(size_t)(i + j + half)] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
    {
        const double inv = 1.0 / (double)n;
        for (auto& x : a)
            x *= inv;
    }
}

} // namespace fireous
