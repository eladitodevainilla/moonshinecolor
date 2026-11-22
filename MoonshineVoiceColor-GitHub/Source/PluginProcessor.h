#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

struct VoiceProfile {
    juce::String name;
    juce::String subtitle;
    float body;
    float color;
    float freshness;
};

class MoonshineVoiceColorProcessor : public juce::AudioProcessor {
public:
    MoonshineVoiceColorProcessor();
    ~MoonshineVoiceColorProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Moonshine Voice Color"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Relearn & Profile Controls
    void startRelearn();
    void applyProfile(int index);
    bool isLearning() const { return learningActive.load(); }
    float getLearnProgress() const { return learnProgress.load(); }
    bool hasLearnedOptions() const { return optionsReady.load(); }
    int getActiveProfileIndex() const { return activeProfileIndex.load(); }
    std::array<VoiceProfile, 3> getSuggestedProfiles() const { return suggestedProfiles; }

    // Live metering for UI
    float getInputLevel() const { return currentInputRms.load(); }
    float getOutputLevel() const { return currentOutputRms.load(); }
    float getDeEsserReduction() const { return currentDeEssGr.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateDSPFilters(float bodyVal, float colorVal, float freshVal);
    void finalizeRelearnAnalysis();

    double currentSampleRate = 44100.0;

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    // Stereo DSP Chains
    Filter hpCleanL, hpCleanR;          // 75Hz subsonic cleanup
    Filter mudDipL, mudDipR;            // 320Hz dynamic mud control
    Filter bodyShelfL, bodyShelfR;      // 185Hz warm vocal body
    Filter colorPeakL, colorPeakR;      // 2.6kHz analog console presence
    Filter airShelfL, airShelfR;        // 12.5kHz silky air band
    Filter sibilanceDetectL, sibilanceDetectR; // 6.8kHz de-esser detector

    // Relearn 4-band spectral analyzer
    Filter analyzeLow, analyzeMud, analyzeMid, analyzeHigh;

    // Parameter smoothers (zero zipper noise)
    juce::SmoothedValue<float> smoothBody, smoothColor, smoothFresh, smoothMix, smoothOut;

    // De-esser & Glue envelope followers
    float deEssEnv = 0.0f;
    float glueEnv  = 0.0f;

    // Relearn state
    std::atomic<bool> learningActive { false };
    std::atomic<bool> optionsReady { false };
    std::atomic<float> learnProgress { 0.0f };
    std::atomic<int> activeProfileIndex { -1 };

    // Metering atomics
    std::atomic<float> currentInputRms { 0.0f };
    std::atomic<float> currentOutputRms { 0.0f };
    std::atomic<float> currentDeEssGr { 0.0f };

    int samplesAnalyzed = 0;
    int targetSamplesToAnalyze = 132300;
    double accumLowEnergy = 0.0;
    double accumMudEnergy = 0.0;
    double accumMidEnergy = 0.0;
    double accumHighEnergy = 0.0;

    std::array<VoiceProfile, 3> suggestedProfiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonshineVoiceColorProcessor)
};
