/*
 * FIReous - DPF-agnostic DSP engine.
 *
 * Default mode is IIR (RBJ biquads, near-zero latency, nonlinear phase).
 * LINEAR mode designs one linear-phase FIR for the EQ/PEQ block (combined
 * when the two modules are adjacent) and reports its group delay.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Params.hpp"
#include "dsp/Biquad.hpp"
#include "dsp/Compressor.hpp"
#include "dsp/DelayLine.hpp"
#include "dsp/EqDesign.hpp"
#include "dsp/FirConvolver.hpp"
#include "dsp/Limiter.hpp"
#include "dsp/Sync.hpp"

namespace fireous {

class Engine
{
public:
    static constexpr int kMaxFirTaps = 32769;

    Engine()
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i].store(getParamInfo(i).def);

        resolveOrder();
        setSampleRate(44100.0);
        rebuildNow();
    }

    void setSampleRate(double sampleRate)
    {
        fSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        fDelay.prepare(fSampleRate);
        fAlign.prepare(fSampleRate);
        fSidechainAlign.prepare(fSampleRate);
        fDryAlign.prepare(fSampleRate);
        fComp.prepare(fSampleRate);
        fLim.prepare(fSampleRate);

        for (auto& ch : fCh)
        {
            ch.eqFir.prepare(kMaxFirTaps);
            ch.peqFir.prepare(kMaxFirTaps);
            ch.toneFir.prepare(kMaxFirTaps);
            ch.eqFir.reset();
            ch.peqFir.reset();
            ch.toneFir.reset();
            for (auto& b : ch.eq) b.reset();
            for (auto& b : ch.peq) b.reset();
            ch.hp1.reset(); ch.hp2.reset();
            ch.lp1.reset(); ch.lp2.reset();
        }

        applyIirAll();
        applyDelay();
        applyComp();
        applyLim();
        fNeedsRebuild.store(true);
    }

    void setBpm(double bpm) noexcept
    {
        fBpm = bpm > 0.0 ? bpm : 120.0;
        applyDelay();
    }

    float getParameterValue(uint32_t index) const noexcept
    {
        if (index >= kParamCount)
            return 0.0f;
        if (isOutputParam(index))
        {
            switch (index)
            {
            case kParamMeterInPeakL:   return fMeterInPeakL.load();
            case kParamMeterInPeakR:   return fMeterInPeakR.load();
            case kParamMeterOutPeakL:  return fMeterOutPeakL.load();
            case kParamMeterOutPeakR:  return fMeterOutPeakR.load();
            case kParamMeterGrComp:    return fMeterGrComp.load();
            case kParamMeterGrLim:     return fMeterGrLim.load();
            case kParamMeterLatency:   return (float)latencySamples();
            case kParamMeterPhaseMode: return fValues[kParamPhaseMode].load();
            default:                   return 0.0f;
            }
        }
        return fValues[index].load();
    }

    void setParameterValue(uint32_t index, float value) noexcept
    {
        if (index >= kParamCount || isOutputParam(index))
            return;

        fValues[index].store(value);

        switch (index)
        {
        case kParamPhaseMode:
        case kParamLinearLatency:
            fNeedsRebuild.store(true);
            break;

        case kParamEqOn:
        case kParamEqGain31: case kParamEqGain63: case kParamEqGain125:
        case kParamEqGain250: case kParamEqGain500: case kParamEqGain1k:
        case kParamEqGain2k: case kParamEqGain4k: case kParamEqGain8k:
        case kParamEqGain16k:
        case kParamPeqOn:
        case kParamPeqType1: case kParamPeqType2: case kParamPeqType3:
        case kParamPeqType4: case kParamPeqType5: case kParamPeqType6:
        case kParamPeqFreq1: case kParamPeqFreq2: case kParamPeqFreq3:
        case kParamPeqFreq4: case kParamPeqFreq5: case kParamPeqFreq6:
        case kParamPeqGain1: case kParamPeqGain2: case kParamPeqGain3:
        case kParamPeqGain4: case kParamPeqGain5: case kParamPeqGain6:
        case kParamPeqQ1: case kParamPeqQ2: case kParamPeqQ3:
        case kParamPeqQ4: case kParamPeqQ5: case kParamPeqQ6:
        case kParamHpOn: case kParamHpFreq: case kParamHpSlope:
        case kParamLpOn: case kParamLpFreq: case kParamLpSlope:
            applyIirAll();
            fNeedsRebuild.store(true);
            break;

        case kParamDelayOn:
        case kParamDelayMode:
        case kParamDelaySync:
        case kParamDelayTimeMs:
        case kParamDelayFeedback:
        case kParamDelayPingPong:
        case kParamDelayMix:
        case kParamDelayTone:
        case kParamAlignLMs:
        case kParamAlignRMs:
            applyDelay();
            break;

        case kParamCompOn:
        case kParamCompThreshold:
        case kParamCompRatio:
        case kParamCompAttack:
        case kParamCompRelease:
        case kParamCompKnee:
        case kParamCompMakeup:
        case kParamCompDetector:
        case kParamCompLink:
        case kParamCompAutoMakeup:
        case kParamCompKey:
            applyComp();
            break;

        case kParamLimOn:
        case kParamLimCeiling:
        case kParamLimRelease:
        case kParamLimLookahead:
        case kParamLimTruePeak:
            applyLim();
            break;

        case kParamChainPosEq:
        case kParamChainPosPeq:
        case kParamChainPosDelay:
        case kParamChainPosComp:
        case kParamChainPosLim:
            resolveOrder();
            fNeedsRebuild.store(true);
            break;

        default:
            break;
        }
    }

    // Called by the worker thread (or synchronously in tests).
    void rebuildNow()
    {
        const bool linear = fValues[kParamPhaseMode].load() > 0.5f;
        if (!linear)
        {
            fCombineTone.store(false);
            fFirLatency.store(0);
            fEqTaps = fPeqTaps = fToneTaps = 0;
            fNeedsRebuild.store(false);
            return;
        }

        const bool eqActive = eqHasEffect();
        const bool peqActive = peqHasEffect();
        if (!eqActive && !peqActive)
        {
            fCombineTone.store(false);
            fFirLatency.store(0);
            fEqTaps = fPeqTaps = fToneTaps = 0;
            fNeedsRebuild.store(false);
            return;
        }

        float orderValues[kStageCount] = {
            fValues[kParamChainPosEq].load(), fValues[kParamChainPosPeq].load(),
            fValues[kParamChainPosDelay].load(), fValues[kParamChainPosComp].load(),
            fValues[kParamChainPosLim].load()
        };
        int order[kStageCount];
        resolveChainOrder(orderValues, order);
        int posEq = -1, posPeq = -1;
        for (int i = 0; i < kStageCount; ++i)
        {
            if (order[i] == kStageEq)  posEq = i;
            if (order[i] == kStagePeq) posPeq = i;
        }
        const bool combine = eqActive && peqActive && std::abs(posEq - posPeq) == 1;

        const float ms = linearLatencyMsFromIndex((int)std::lround(fValues[kParamLinearLatency].load()));
        const int activeCount = (eqActive ? 1 : 0) + (peqActive ? 1 : 0);
        const int taps = combine
                       ? tapsForLatencyMs(ms, fSampleRate)
                       : tapsForLatencyMs(ms / (float)std::max(1, activeCount), fSampleRate);

        fCombineTone.store(combine);
        fEqTaps = fPeqTaps = fToneTaps = 0;

        if (combine)
        {
            const auto h = designEqFir(makeEqSettings(true, true), taps, fSampleRate);
            fCh[0].toneFir.setCoefficients(h);
            fCh[1].toneFir.setCoefficients(h);
            fToneTaps = taps;
            fFirLatency.store((taps - 1) / 2);
        }
        else
        {
            int latency = 0;
            if (eqActive)
            {
                const auto h = designEqFir(makeEqSettings(true, false), taps, fSampleRate);
                fCh[0].eqFir.setCoefficients(h);
                fCh[1].eqFir.setCoefficients(h);
                fEqTaps = taps;
                latency += (taps - 1) / 2;
            }
            if (peqActive)
            {
                const auto h = designEqFir(makeEqSettings(false, true), taps, fSampleRate);
                fCh[0].peqFir.setCoefficients(h);
                fCh[1].peqFir.setCoefficients(h);
                fPeqTaps = taps;
                latency += (taps - 1) / 2;
            }
            fFirLatency.store(latency);
        }

        fNeedsRebuild.store(false);
    }

    bool needsRebuild() const noexcept { return fNeedsRebuild.load(); }
    bool isLinearMode() const noexcept { return fValues[kParamPhaseMode].load() > 0.5f; }

    uint32_t latencySamples() const noexcept
    {
        if (fValues[kParamBypass].load() > 0.5f)
            return 0;
        const int lim = (fValues[kParamLimOn].load() > 0.5f) ? fLim.getLookaheadSamples() : 0;
        if (!isLinearMode())
            return (uint32_t)std::max(0, lim);
        return (uint32_t)std::max(0, fFirLatency.load() + lim);
    }

    void processBlock(const float* inL, const float* inR,
                      const float* scL, const float* scR,
                      float* outL, float* outR, uint32_t frames) noexcept
    {
        if (frames == 0)
            return;

        const bool bypass = fValues[kParamBypass].load() > 0.5f;
        if (bypass)
        {
            for (uint32_t i = 0; i < frames; ++i)
            {
                outL[i] = inL[i];
                outR[i] = inR[i];
            }
            updateMeters(inL, inR, outL, outR, frames, 0.0f, 0.0f);
            return;
        }

        const bool linear = isLinearMode();
        const bool eqOn = fValues[kParamEqOn].load() > 0.5f;
        const bool peqOn = fValues[kParamPeqOn].load() > 0.5f;
        const bool delayOn = fValues[kParamDelayOn].load() > 0.5f;
        const bool compOn = fValues[kParamCompOn].load() > 0.5f;
        const bool limOn = fValues[kParamLimOn].load() > 0.5f;
        const bool externalKey = fValues[kParamCompKey].load() > 0.5f && scL != nullptr;
        const bool combine = fCombineTone.load();

        const bool eqActive = linear && eqOn && eqHasEffect();
        const bool peqActive = linear && peqOn && peqHasEffect();

        for (auto& ch : fCh)
        {
            ch.eqFir.beginBlock();
            ch.peqFir.beginBlock();
            ch.toneFir.beginBlock();
        }

        const float firLatencyMs = linear ? (float)fFirLatency.load() * 1000.0f / (float)fSampleRate : 0.0f;
        fSidechainAlign.setDelaysMs(firLatencyMs, firLatencyMs);

        const bool delta = fValues[kParamMonitor].load() > 0.5f;
        const float totalLatencyMs = (float)latencySamples() * 1000.0f / (float)fSampleRate;
        fDryAlign.setDelaysMs(totalLatencyMs, totalLatencyMs);

        const float inputGain = std::pow(10.0f, fValues[kParamInputTrim].load() / 20.0f);
        const float outputGain = std::pow(10.0f, fValues[kParamOutputGain].load() / 20.0f);
        const int delayMode = (int)std::lround(fValues[kParamDelayMode].load());

        float grComp = 0.0f, grLim = 0.0f;

        for (uint32_t i = 0; i < frames; ++i)
        {
            float l = inL[i] * inputGain;
            float r = inR[i] * inputGain;

            float dryL = 0.0f, dryR = 0.0f;
            if (delta)
                fDryAlign.process(inL[i], inR[i], dryL, dryR);

            float keyL = l, keyR = r;
            if (externalKey)
            {
                float sl = scL[i];
                float sr = (scR != nullptr) ? scR[i] : sl;
                fSidechainAlign.process(sl, sr, sl, sr);
                keyL = sl;
                keyR = sr;
            }

            bool toneDone = false;

            for (int si = 0; si < kStageCount; ++si)
            {
                switch (fChainOrder[si])
                {
                case kStageEq:
                    if (linear)
                    {
                        if (combine)
                        {
                            if (!toneDone)
                            {
                                l = fCh[0].toneFir.process(l);
                                r = fCh[1].toneFir.process(r);
                                toneDone = true;
                            }
                        }
                        else if (eqActive)
                        {
                            l = fCh[0].eqFir.process(l);
                            r = fCh[1].eqFir.process(r);
                        }
                    }
                    else if (eqOn)
                    {
                        for (auto& b : fCh[0].eq) l = b.process(l);
                        for (auto& b : fCh[1].eq) r = b.process(r);
                    }
                    break;

                case kStagePeq:
                    if (linear)
                    {
                        if (combine)
                        {
                            if (!toneDone)
                            {
                                l = fCh[0].toneFir.process(l);
                                r = fCh[1].toneFir.process(r);
                                toneDone = true;
                            }
                        }
                        else if (peqActive)
                        {
                            l = fCh[0].peqFir.process(l);
                            r = fCh[1].peqFir.process(r);
                        }
                    }
                    else if (peqOn)
                    {
                        for (auto& b : fCh[0].peq) l = b.process(l);
                        for (auto& b : fCh[1].peq) r = b.process(r);
                        l = fCh[0].hp1.process(l); r = fCh[1].hp1.process(r);
                        l = fCh[0].hp2.process(l); r = fCh[1].hp2.process(r);
                        l = fCh[0].lp1.process(l); r = fCh[1].lp1.process(r);
                        l = fCh[0].lp2.process(l); r = fCh[1].lp2.process(r);
                    }
                    break;

                case kStageDelay:
                    if (delayOn)
                    {
                        if (delayMode != 1)
                            fDelay.process(l, r, l, r);
                        if (delayMode != 0)
                            fAlign.process(l, r, l, r);
                    }
                    break;

                case kStageComp:
                    if (compOn)
                    {
                        if (!externalKey)
                        {
                            keyL = l;
                            keyR = r;
                        }
                        float kl = keyL, kr = keyR;
                        fComp.process(l, r, kl, kr);
                        grComp = std::max(grComp, fComp.getGainReductionDb());
                    }
                    break;

                case kStageLim:
                    if (limOn)
                    {
                        fLim.process(l, r);
                        grLim = std::max(grLim, fLim.getGainReductionDb());
                    }
                    break;

                default:
                    break;
                }
            }

            outL[i] = l * outputGain;
            outR[i] = r * outputGain;
            if (delta)
            {
                outL[i] -= dryL * inputGain * outputGain;
                outR[i] -= dryR * inputGain * outputGain;
            }
        }

        updateMeters(inL, inR, outL, outR, frames, grComp, grLim);
    }

private:
    struct ChannelState
    {
        Biquad eq[kNumGraphicBands];
        Biquad peq[kNumPeqBands];
        Biquad hp1, hp2, lp1, lp2;
        FirConvolver eqFir, peqFir, toneFir;
    };

    float value(uint32_t index) const noexcept { return fValues[index].load(); }

    void resolveOrder() noexcept
    {
        float vals[kStageCount] = {
            value(kParamChainPosEq), value(kParamChainPosPeq), value(kParamChainPosDelay),
            value(kParamChainPosComp), value(kParamChainPosLim)
        };
        resolveChainOrder(vals, fChainOrder);
    }

    bool eqHasEffect() const noexcept
    {
        if (value(kParamEqOn) <= 0.5f)
            return false;
        static constexpr uint32_t first = kParamEqGain31;
        for (uint32_t i = 0; i < kNumGraphicBands; ++i)
            if (std::fabs(value(first + i)) > 1e-4f)
                return true;
        return false;
    }

    bool peqHasEffect() const noexcept
    {
        if (value(kParamPeqOn) <= 0.5f)
            return false;
        static constexpr uint32_t firstGain = kParamPeqGain1;
        for (uint32_t i = 0; i < kNumPeqBands; ++i)
            if (std::fabs(value(firstGain + i)) > 1e-4f)
                return true;
        return value(kParamHpOn) > 0.5f || value(kParamLpOn) > 0.5f;
    }

    EqSettings makeEqSettings(bool includeEq, bool includePeq) const noexcept
    {
        EqSettings s;
        s.eqOn = includeEq && value(kParamEqOn) > 0.5f;
        static constexpr uint32_t firstEq = kParamEqGain31;
        for (int i = 0; i < kNumGraphicBands; ++i)
            s.eqGains[i] = value(firstEq + (uint32_t)i);

        s.peqOn = includePeq && value(kParamPeqOn) > 0.5f;
        static constexpr uint32_t type0 = kParamPeqType1;
        static constexpr uint32_t freq0 = kParamPeqFreq1;
        static constexpr uint32_t gain0 = kParamPeqGain1;
        static constexpr uint32_t q0 = kParamPeqQ1;
        for (int i = 0; i < kNumPeqBands; ++i)
        {
            s.peq[i].type = (int)std::lround(value(type0 + (uint32_t)i));
            s.peq[i].freq = value(freq0 + (uint32_t)i);
            s.peq[i].gainDb = value(gain0 + (uint32_t)i);
            s.peq[i].q = value(q0 + (uint32_t)i);
        }

        s.hpOn = value(kParamHpOn) > 0.5f;
        s.hpFreq = value(kParamHpFreq);
        s.hpSlope = (int)std::lround(value(kParamHpSlope));
        s.lpOn = value(kParamLpOn) > 0.5f;
        s.lpFreq = value(kParamLpFreq);
        s.lpSlope = (int)std::lround(value(kParamLpSlope));
        return s;
    }

    void applyIirAll() noexcept
    {
        const bool eqOn = value(kParamEqOn) > 0.5f;
        const bool peqOn = value(kParamPeqOn) > 0.5f;
        const bool hpOn = peqOn && value(kParamHpOn) > 0.5f;
        const bool lpOn = peqOn && value(kParamLpOn) > 0.5f;
        const bool hp24 = value(kParamHpSlope) > 0.5f;
        const bool lp24 = value(kParamLpSlope) > 0.5f;

        static constexpr uint32_t firstEq = kParamEqGain31;
        static constexpr uint32_t type0 = kParamPeqType1;
        static constexpr uint32_t freq0 = kParamPeqFreq1;
        static constexpr uint32_t gain0 = kParamPeqGain1;
        static constexpr uint32_t q0 = kParamPeqQ1;

        for (auto& ch : fCh)
        {
            for (int i = 0; i < kNumGraphicBands; ++i)
            {
                if (eqOn)
                    ch.eq[i].setBell(fSampleRate, kGraphicFreqs[i], value(firstEq + (uint32_t)i), 1.4f);
                else
                    ch.eq[i].setIdentity();
            }

            for (int i = 0; i < kNumPeqBands; ++i)
            {
                if (peqOn)
                {
                    const int type = (int)std::lround(value(type0 + (uint32_t)i));
                    const Biquad::Type t = (type == 1) ? Biquad::Type::LowShelf
                                         : (type == 2) ? Biquad::Type::HighShelf
                                                       : Biquad::Type::Bell;
                    ch.peq[i].set(t, fSampleRate, value(freq0 + (uint32_t)i),
                                  value(gain0 + (uint32_t)i), value(q0 + (uint32_t)i));
                }
                else
                {
                    ch.peq[i].setIdentity();
                }
            }

            if (hpOn)
            {
                ch.hp1.setHighpass(fSampleRate, value(kParamHpFreq), 0.7071f);
                if (hp24) ch.hp2.setHighpass(fSampleRate, value(kParamHpFreq), 0.7071f);
                else ch.hp2.setIdentity();
            }
            else
            {
                ch.hp1.setIdentity();
                ch.hp2.setIdentity();
            }

            if (lpOn)
            {
                ch.lp1.setLowpass(fSampleRate, value(kParamLpFreq), 0.7071f);
                if (lp24) ch.lp2.setLowpass(fSampleRate, value(kParamLpFreq), 0.7071f);
                else ch.lp2.setIdentity();
            }
            else
            {
                ch.lp1.setIdentity();
                ch.lp2.setIdentity();
            }
        }
    }

    void applyDelay() noexcept
    {
        const float seconds = delaySecondsFromSync(value(kParamDelaySync), fBpm, value(kParamDelayTimeMs));
        fDelay.setTimeSeconds(seconds);
        fDelay.setFeedback(value(kParamDelayFeedback));
        fDelay.setPingPong(value(kParamDelayPingPong));
        fDelay.setMix(value(kParamDelayMix));
        fDelay.setTone(value(kParamDelayTone));
        fAlign.setDelaysMs(value(kParamAlignLMs), value(kParamAlignRMs));
    }

    void applyComp() noexcept
    {
        fComp.setThreshold(value(kParamCompThreshold));
        fComp.setRatio(value(kParamCompRatio));
        fComp.setAttackMs(value(kParamCompAttack));
        fComp.setReleaseMs(value(kParamCompRelease));
        fComp.setKnee(value(kParamCompKnee));
        fComp.setMakeup(value(kParamCompMakeup));
        fComp.setDetector(value(kParamCompDetector) > 0.5f);
        fComp.setLink(value(kParamCompLink));
        fComp.setAutoMakeup(value(kParamCompAutoMakeup) > 0.5f);
    }

    void applyLim() noexcept
    {
        fLim.setCeilingDb(value(kParamLimCeiling));
        fLim.setReleaseMs(value(kParamLimRelease));
        fLim.setLookaheadMs(value(kParamLimLookahead));
        fLim.setTruePeak(value(kParamLimTruePeak) > 0.5f);
    }

    void updateMeters(const float* inL, const float* inR,
                      const float* outL, const float* outR,
                      uint32_t frames, float grComp, float grLim) noexcept
    {
        float inPeakL = 0.0f, inPeakR = 0.0f, outPeakL = 0.0f, outPeakR = 0.0f;
        for (uint32_t i = 0; i < frames; ++i)
        {
            inPeakL = std::max(inPeakL, std::fabs(inL[i]));
            inPeakR = std::max(inPeakR, std::fabs(inR[i]));
            outPeakL = std::max(outPeakL, std::fabs(outL[i]));
            outPeakR = std::max(outPeakR, std::fabs(outR[i]));
        }
        fMeterInPeakL.store(inPeakL);
        fMeterInPeakR.store(inPeakR);
        fMeterOutPeakL.store(outPeakL);
        fMeterOutPeakR.store(outPeakR);
        fMeterGrComp.store(grComp);
        fMeterGrLim.store(grLim);
    }

    std::atomic<float> fValues[kParamCount];
    std::atomic<bool> fNeedsRebuild { true };
    std::atomic<bool> fCombineTone { false };
    std::atomic<int> fFirLatency { 0 };

    double fSampleRate = 44100.0;
    double fBpm = 120.0;
    int fChainOrder[kStageCount] = { kStageEq, kStagePeq, kStageDelay, kStageComp, kStageLim };

    ChannelState fCh[2];
    StereoDelay fDelay;
    StereoAlign fAlign;
    StereoAlign fSidechainAlign;
    StereoAlign fDryAlign;
    Compressor fComp;
    Limiter fLim;

    int fEqTaps = 0, fPeqTaps = 0, fToneTaps = 0;

    std::atomic<float> fMeterInPeakL { 0.0f }, fMeterInPeakR { 0.0f };
    std::atomic<float> fMeterOutPeakL { 0.0f }, fMeterOutPeakR { 0.0f };
    std::atomic<float> fMeterGrComp { 0.0f }, fMeterGrLim { 0.0f };
};

} // namespace fireous
