/*
 * FIReous - DPF plugin wrapper. The DSP lives in Engine.hpp and stays
 * DPF-agnostic so the same code can be tested standalone.
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "Engine.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

START_NAMESPACE_DISTRHO

using namespace fireous;

class FireousPlugin : public Plugin
{
public:
    FireousPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        {
            std::lock_guard<std::mutex> lock(fDesignMutex);
            fEngine.setSampleRate(getSampleRate());
        }
        fLastLatency = fEngine.latencySamples();
        setLatency(fLastLatency);

        fWorkerRunning.store(true);
        fWorker = std::thread([this]() {
            while (fWorkerRunning.load(std::memory_order_relaxed))
            {
                if (fEngine.needsRebuild())
                {
                    std::lock_guard<std::mutex> lock(fDesignMutex);
                    fEngine.rebuildNow();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
            }
        });
    }

    ~FireousPlugin() override
    {
        fWorkerRunning.store(false);
        if (fWorker.joinable())
            fWorker.join();
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "FIReous"; }
    const char* getDescription() const override
    {
        return "IIR / linear-phase FIR EQ, delay, compressor and limiter. "
               "LINEAR mode keeps the EQ and PEQ phase linear; IIR mode is the "
               "near-zero-latency default.";
    }
    const char* getMaker() const override { return "FIReous"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    // ---------------------------------------------------------------------
    // Init

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        Plugin::initAudioPort(input, index, port);
        if (input && index >= 2)
        {
            port.hints |= kAudioPortIsSidechain;
            port.name = (index == 2) ? "Sidechain Left" : "Sidechain Right";
            port.symbol = (index == 2) ? "sidechain_l" : "sidechain_r";
        }
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const ParamInfo& info = getParamInfo(index);

        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;

        if (isHostPosition(index))
        {
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = paramToHost(index, info.def);
        }

        if (isOutputParam(index))
        {
            parameter.hints = kParameterIsOutput;
            return;
        }

        parameter.hints = kParameterIsAutomatable;
        if (info.stepped)
        {
            parameter.hints |= kParameterIsInteger;
            if (info.max <= 1.0f)
                parameter.hints |= kParameterIsBoolean;
        }
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
    {
        return paramToHost(index, fEngine.getParameterValue(index));
    }

    void setParameterValue(uint32_t index, float value) override
    {
        value = paramFromHost(index, value);
        fEngine.setParameterValue(index, value);
    }

    // ---------------------------------------------------------------------
    // Audio Processing

    void activate() override
    {
        std::lock_guard<std::mutex> lock(fDesignMutex);
        fEngine.setSampleRate(getSampleRate());
        fLastLatency = fEngine.latencySamples();
        setLatency(fLastLatency);
    }

    void sampleRateChanged(double newSampleRate) override
    {
        std::lock_guard<std::mutex> lock(fDesignMutex);
        fEngine.setSampleRate(newSampleRate);
        fLastLatency = fEngine.latencySamples();
        setLatency(fLastLatency);
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& timePos = getTimePosition();
        const double bpm = timePos.bbt.valid && timePos.bbt.beatsPerMinute > 0.0
                         ? timePos.bbt.beatsPerMinute : 120.0;
        fEngine.setBpm(bpm);

        const float* inL = inputs[0];
        const float* inR = inputs[1];
        const float* scL = (DISTRHO_PLUGIN_NUM_INPUTS > 2) ? inputs[2] : nullptr;
        const float* scR = (DISTRHO_PLUGIN_NUM_INPUTS > 3) ? inputs[3] : nullptr;

        const uint32_t latency = fEngine.latencySamples();
        if (latency != fLastLatency)
        {
            fLastLatency = latency;
            setLatency(latency);
        }

        fEngine.processBlock(inL, inR, scL, scR, outputs[0], outputs[1], frames);
    }

private:
    Engine fEngine;
    mutable std::mutex fDesignMutex;
    std::thread fWorker;
    std::atomic<bool> fWorkerRunning { false };
    uint32_t fLastLatency = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireousPlugin)
};

// -----------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new FireousPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
