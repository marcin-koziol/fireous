#include "test_common.hpp"

#include "Engine.hpp"
#include "dsp/EqDesign.hpp"
#include "dsp/FirDesign.hpp"

#include <cmath>
#include <vector>

using namespace fireous;
using namespace test;

static void set(Engine& e, uint32_t p, float v) { e.setParameterValue(p, v); }

static void testIirFlat()
{
    Engine e;
    e.setSampleRate(48000.0);
    e.rebuildNow();

    const int n = 1024;
    std::vector<float> in((size_t)n, 0.0f), outL((size_t)n), outR((size_t)n);
    in[0] = 0.1f;
    e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);

    CHECK_NEAR(outL[0], 0.1f, 1e-6f);
    CHECK_NEAR(outR[0], 0.1f, 1e-6f);
    for (int i = 1; i < n; ++i)
    {
        CHECK_NEAR(outL[(size_t)i], 0.0f, 1e-6f);
        CHECK_NEAR(outR[(size_t)i], 0.0f, 1e-6f);
    }
    CHECK_NEAR(e.latencySamples(), 0.0f, 0.0f);
}

static void testLinearLatencyAndIdentity()
{
    Engine e;
    e.setSampleRate(48000.0);
    set(e, kParamPhaseMode, 1.0f);
    set(e, kParamLinearLatency, 2.0f); // 10 ms
    set(e, kParamEqGain1k, 0.01f);     // force a FIR design; nearly flat
    e.rebuildNow();

    const uint32_t latency = e.latencySamples();
    CHECK_NEAR(latency, 480.0f, 1.0f);

    const int n = 2048;
    std::vector<float> in((size_t)n, 0.0f), outL((size_t)n), outR((size_t)n);
    in[0] = 0.1f;
    e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);

    size_t peakIdx = 0;
    float peakVal = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (std::fabs(outL[(size_t)i]) > peakVal)
        {
            peakVal = std::fabs(outL[(size_t)i]);
            peakIdx = (size_t)i;
        }
    }
    CHECK_NEAR(peakIdx, latency, 1.0f);
    CHECK_NEAR(peakVal, 0.1f, 0.005f);
}

static void testPeqGainIirAndLinear()
{
    const double sr = 48000.0;
    const int n = 48000 * 2;
    const float freq = 1000.0f;
    const float amp = 0.1f;

    auto measure = [&](bool linear) {
        Engine e;
        e.setSampleRate(sr);
        set(e, kParamLimOn, 0.0f);
        set(e, kParamCompOn, 0.0f);
        set(e, kParamPhaseMode, linear ? 1.0f : 0.0f);
        set(e, kParamLinearLatency, 2.0f);
        set(e, kParamPeqGain2, 6.0f);
        set(e, kParamPeqFreq2, 1000.0f);
        set(e, kParamPeqQ2, 1.0f);
        e.rebuildNow();

        std::vector<float> in((size_t)n), outL((size_t)n), outR((size_t)n);
        for (int i = 0; i < n; ++i)
            in[(size_t)i] = amp * std::sin(2.0 * M_PI * freq * (double)i / sr);

        e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);
        const float r = rms(outL, (size_t)(n - 4096));
        return r / (amp / std::sqrt(2.0f));
    };

    const float iirGain = measure(false);
    const float firGain = measure(true);
    std::printf("peq 1k +6 dB: IIR gain %.4f, LINEAR gain %.4f\n", iirGain, firGain);
    CHECK(iirGain > 1.85f && iirGain < 2.15f);
    CHECK(firGain > 1.80f && firGain < 2.20f);
}

static void testCompressor()
{
    const double sr = 48000.0;
    const int n = 48000;
    const float amp = 0.316f; // -10 dBFS peak

    Engine e;
    e.setSampleRate(sr);
    set(e, kParamLimOn, 0.0f);
    set(e, kParamCompOn, 1.0f);
    set(e, kParamCompThreshold, -20.0f);
    set(e, kParamCompRatio, 4.0f);
    set(e, kParamCompAttack, 1.0f);
    set(e, kParamCompRelease, 50.0f);
    set(e, kParamCompKnee, 0.0f);
    set(e, kParamCompMakeup, 0.0f);
    set(e, kParamCompDetector, 1.0f); // peak
    set(e, kParamCompLink, 1.0f);
    set(e, kParamCompAutoMakeup, 0.0f);

    std::vector<float> in((size_t)n), outL((size_t)n), outR((size_t)n);
    for (int i = 0; i < n; ++i)
        in[(size_t)i] = amp * std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);

    e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);
    const float outPeak = peak(outL, (size_t)(n - 4096));
    std::printf("compressor: input peak %.4f, output peak %.4f\n", amp, outPeak);
    // -10 dB in, -20 dB threshold, 4:1 => -17.5 dB out => ~0.133.
    CHECK(outPeak > 0.11f && outPeak < 0.16f);
}

static void testLimiter()
{
    const double sr = 48000.0;
    const int n = 24000;
    const float amp = 0.9f;

    Engine e;
    e.setSampleRate(sr);
    set(e, kParamCompOn, 0.0f);
    set(e, kParamLimOn, 1.0f);
    set(e, kParamLimCeiling, -6.0f);
    set(e, kParamLimLookahead, 1.0f);
    set(e, kParamLimTruePeak, 0.0f);

    std::vector<float> in((size_t)n), outL((size_t)n), outR((size_t)n);
    for (int i = 0; i < n; ++i)
        in[(size_t)i] = amp * std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);

    e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);
    const float p = peak(outL, 1024);
    std::printf("limiter: output peak %.6f (ceiling %.6f), latency %u\n", p, std::pow(10.0f, -6.0f / 20.0f), e.latencySamples());
    CHECK(p <= std::pow(10.0f, -6.0f / 20.0f) + 1e-4f);
}

static void testDelayAlign()
{
    Engine e;
    e.setSampleRate(48000.0);
    set(e, kParamLimOn, 0.0f);
    set(e, kParamCompOn, 0.0f);
    set(e, kParamDelayOn, 1.0f);
    set(e, kParamDelayMode, 1.0f); // align only
    set(e, kParamAlignLMs, 5.0f);
    set(e, kParamAlignRMs, 0.0f);

    const int n = 1024;
    std::vector<float> in((size_t)n, 0.0f), outL((size_t)n), outR((size_t)n);
    in[0] = 1.0f;
    e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);

    CHECK_NEAR(outR[0], 1.0f, 1e-5f);
    CHECK_NEAR(outL[240], 1.0f, 1e-4f);
    for (int i = 0; i < 240; ++i)
        CHECK_NEAR(outL[(size_t)i], 0.0f, 1e-5f);
}

static void testReorderInvariance()
{
    const int n = 512;
    std::vector<float> in((size_t)n, 0.0f), aL((size_t)n), aR((size_t)n), bL((size_t)n), bR((size_t)n);
    in[0] = 0.25f;

    auto run = [&](float eqPos, float peqPos, std::vector<float>& outL, std::vector<float>& outR) {
        Engine e;
        e.setSampleRate(48000.0);
        set(e, kParamLimOn, 0.0f);
        set(e, kParamCompOn, 0.0f);
        set(e, kParamDelayOn, 0.0f);
        set(e, kParamEqGain1k, 6.0f);
        set(e, kParamPeqGain3, -3.0f);
        set(e, kParamPeqFreq3, 2000.0f);
        set(e, kParamChainPosEq, eqPos);
        set(e, kParamChainPosPeq, peqPos);
        e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);
    };

    run(0.0f, 1.0f, aL, aR);
    run(1.0f, 0.0f, bL, bR);

    for (int i = 0; i < n; ++i)
    {
        CHECK_NEAR(aL[(size_t)i], bL[(size_t)i], 1e-4f);
        CHECK_NEAR(aR[(size_t)i], bR[(size_t)i], 1e-4f);
    }
}


static void testDeltaMonitor()
{
    Engine e;
    e.setSampleRate(48000.0);
    set(e, kParamLimOn, 0.0f);
    set(e, kParamCompOn, 0.0f);
    set(e, kParamMonitor, 1.0f);

    const int n = 4096;
    std::vector<float> in((size_t)n), outL((size_t)n), outR((size_t)n);
    for (int i = 0; i < n; ++i)
        in[(size_t)i] = 0.25f * std::sin(2.0 * M_PI * 440.0 * (double)i / 48000.0);

    e.processBlock(in.data(), in.data(), nullptr, nullptr, outL.data(), outR.data(), (uint32_t)n);
    CHECK(peak(outL, 256) < 1e-4f);
    CHECK(peak(outR, 256) < 1e-4f);
}

static void testExternalSidechain()
{
    const double sr = 48000.0;
    const int n = 24000;

    auto measure = [&](bool external) {
        Engine e;
        e.setSampleRate(sr);
        set(e, kParamLimOn, 0.0f);
        set(e, kParamCompOn, 1.0f);
        set(e, kParamCompThreshold, -20.0f);
        set(e, kParamCompRatio, 4.0f);
        set(e, kParamCompAttack, 1.0f);
        set(e, kParamCompRelease, 50.0f);
        set(e, kParamCompKnee, 0.0f);
        set(e, kParamCompDetector, 1.0f);
        set(e, kParamCompLink, 1.0f);
        set(e, kParamCompKey, external ? 1.0f : 0.0f);

        std::vector<float> mainSig((size_t)n), sideSig((size_t)n), outL((size_t)n), outR((size_t)n);
        for (int i = 0; i < n; ++i)
        {
            mainSig[(size_t)i] = 0.05f * std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);
            sideSig[(size_t)i] = 0.316f * std::sin(2.0 * M_PI * 1000.0 * (double)i / sr);
        }

        e.processBlock(mainSig.data(), mainSig.data(),
                       external ? sideSig.data() : nullptr,
                       external ? sideSig.data() : nullptr,
                       outL.data(), outR.data(), (uint32_t)n);
        return peak(outL, (size_t)(n - 4096));
    };

    const float internalPeak = measure(false);
    const float externalPeak = measure(true);
    std::printf("sidechain: internal peak %.5f, external peak %.5f\n", internalPeak, externalPeak);
    CHECK(internalPeak > 0.045f);
    CHECK(externalPeak < 0.030f);
}

static void testFirSanity()
{
    // Flat magnitude -> delta at the group delay.
    const int taps = 961;
    const int K = 8192;
    std::vector<double> mag((size_t)(K / 2 + 1), 1.0);
    const auto h = designLinearPhaseFromMagnitude(mag, taps, 8.6);
    CHECK((int)h.size() == taps);
    const int M = (taps - 1) / 2;
    CHECK_NEAR(h[(size_t)M], 1.0f, 1e-3f);
    for (int i = 0; i < taps; ++i)
        if (std::abs(i - M) > 4)
            CHECK_NEAR(h[(size_t)i], 0.0f, 1e-3f);

    // Symmetry.
    for (int i = 0; i < taps; ++i)
        CHECK_NEAR(h[(size_t)i], h[(size_t)(taps - 1 - i)], 1e-6f);
}

int main()
{
    testIirFlat();
    testLinearLatencyAndIdentity();
    testPeqGainIirAndLinear();
    testCompressor();
    testLimiter();
    testDelayAlign();
    testReorderInvariance();
    testDeltaMonitor();
    testExternalSidechain();
    testFirSanity();
    return finish();
}
