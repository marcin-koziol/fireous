/*
 * FIReous - direct-form FIR convolver with lock-free coefficient handoff.
 *
 * The worker thread publishes a new coefficient vector; the audio thread
 * picks it up once per block in beginBlock(). No allocation happens in
 * process().
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace fireous {

class FirConvolver
{
public:
    struct Coefficients
    {
        std::vector<float> h;
    };

    void prepare(int maxTaps)
    {
        fMaxTaps = std::max(1, maxTaps);
        fHistory.assign((size_t)fMaxTaps + 1, 0.0f);
        fWrite = 0;
        reset();
    }

    void reset() noexcept
    {
        std::fill(fHistory.begin(), fHistory.end(), 0.0f);
        fWrite = 0;
    }

    void setCoefficients(std::vector<float> h)
    {
        auto coeffs = std::make_shared<const Coefficients>(Coefficients{ std::move(h) });
        std::atomic_store(&fCoeffs, coeffs);
    }

    void beginBlock() noexcept
    {
        fActive = std::atomic_load(&fCoeffs);
    }

    float process(float x) noexcept
    {
        if (!fActive || fActive->h.empty() || fHistory.empty())
            return x;

        const int taps = std::min((int)fActive->h.size(), fMaxTaps);
        const float* h = fActive->h.data();
        const size_t size = fHistory.size();

        fHistory[fWrite] = x;
        float y = 0.0f;
        size_t idx = fWrite;
        for (int k = 0; k < taps; ++k)
        {
            y += h[k] * fHistory[idx];
            idx = (idx == 0) ? size - 1 : idx - 1;
        }
        fWrite = (fWrite + 1) % size;
        return y;
    }

    int taps() const noexcept
    {
        const auto c = std::atomic_load(&fCoeffs);
        return c ? (int)c->h.size() : 0;
    }

private:
    std::vector<float> fHistory;
    int fMaxTaps = 1;
    size_t fWrite = 0;
    std::shared_ptr<const Coefficients> fCoeffs;
    std::shared_ptr<const Coefficients> fActive;
};

} // namespace fireous
