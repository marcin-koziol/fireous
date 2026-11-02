#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace test {

inline int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++test::failures; \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    const double _a = (double)(a); \
    const double _b = (double)(b); \
    if (std::fabs(_a - _b) > (double)(eps)) { \
        std::printf("FAIL %s:%d: %s=%.9g != %s=%.9g (eps %.3g)\n", \
                    __FILE__, __LINE__, #a, _a, #b, _b, (double)(eps)); \
        ++test::failures; \
    } \
} while (0)

inline float rms(const std::vector<float>& x, size_t start)
{
    double sum = 0.0;
    size_t n = 0;
    for (size_t i = start; i < x.size(); ++i)
    {
        sum += (double)x[i] * (double)x[i];
        ++n;
    }
    return n ? (float)std::sqrt(sum / (double)n) : 0.0f;
}

inline float peak(const std::vector<float>& x, size_t start = 0)
{
    float p = 0.0f;
    for (size_t i = start; i < x.size(); ++i)
        p = std::max(p, std::fabs(x[i]));
    return p;
}

inline int finish()
{
    if (test::failures == 0)
        std::printf("PASS\n");
    else
        std::printf("FAILURES: %d\n", test::failures);
    return test::failures ? 1 : 0;
}

} // namespace test
