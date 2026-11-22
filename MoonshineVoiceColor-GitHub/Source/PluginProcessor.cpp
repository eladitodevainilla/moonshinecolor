#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

MoonshineVoiceColorProcessor::MoonshineVoiceColorProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {

    suggestedProfiles[0] = { "CRISP & FRESH",  "Brillo & Aire Moderno", 45.0f, 52.0f, 78.0f };
    suggestedProfiles[1] = { "PRO BODY",       "Peso & Presencia",      74.0f, 62.0f, 46.0f };
    suggestedProfiles[2] = { "STUDIO COLOR",   "Balance Profesional",   58.0f, 68.0f, 64.0f };
}

juce::AudioProcessorValueTreeState::ParameterLayout MoonshineVoiceColorProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"body", 1}, "Body (Cuerpo)", 0.0f, 100.0f, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"color", 1}, "Color (Calidez)", 0.0f, 100.0f, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"freshness", 1}, "Freshness (Brillo)", 0.0f, 100.0f, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Mix", 0.0f, 100.0f, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"output", 1}, "Output Gain", -12.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    return { params.begin(), params.end() };
}

void MoonshineVoiceColorProcessor::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate;
    targetSamplesToAnalyze = static_cast<int>(sampleRate * 3.0);

    hpCleanL.reset(); hpCleanR.reset();
    mudDipL.reset();  mudDipR.reset();
    bodyShelfL.reset(); bodyShelfR.reset();
    colorPeakL.reset(); colorPeakR.reset();
    airShelfL.reset();  airShelfR.reset();
    sibilanceDetectL.reset(); sibilanceDetectR.reset();

    analyzeLow.reset(); analyzeMud.reset(); analyzeMid.reset(); analyzeHigh.reset();

    *hpCleanL.coefficients = *Coeffs::makeHighPass(sampleRate, 75.0f, 0.707f);
    *hpCleanR.coefficients = *hpCleanL.coefficients;

    *sibilanceDetectL.coefficients = *Coeffs::makeBandPass(sampleRate, 6800.0f, 2.2f);
    *sibilanceDetectR.coefficients = *sibilanceDetectL.coefficients;

    // 4-band spectral analysis filters for RELEARN
    *analyzeLow.coefficients  = *Coeffs::makeLowPass(sampleRate, 260.0f, 0.707f);
    *analyzeMud.coefficients  = *Coeffs::makeBandPass(sampleRate, 420.0f, 1.4f);
    *analyzeMid.coefficients  = *Coeffs::makeBandPass(sampleRate, 2200.0f, 1.0f);
    *analyzeHigh.coefficients = *Coeffs::makeHighPass(sampleRate, 7500.0f, 0.707f);

    smoothBody.reset(sampleRate, 0.02);
    smoothColor.reset(sampleRate, 0.02);
    smoothFresh.reset(sampleRate, 0.02);
    smoothMix.reset(sampleRate, 0.02);
    smoothOut.reset(sampleRate, 0.02);

    deEssEnv = 0.0f;
    glueEnv  = 0.0f;

    updateDSPFilters(0.5f, 0.5f, 0.5f);
}

void MoonshineVoiceColorProcessor::releaseResources() {}

void MoonshineVoiceColorProcessor::updateDSPFilters(float bodyVal, float colorVal, float freshVal) {
    // 1. Body: Low-Shelf at 185 Hz (-1.5 dB to +5.5 dB)
    float bodyGainDb = (bodyVal - 0.22f) * 7.0f;
    *bodyShelfL.coefficients = *Coeffs::makeLowShelf(currentSampleRate, 185.0f, 0.65f, juce::Decibels::decibelsToGain(bodyGainDb));
    *bodyShelfR.coefficients = *bodyShelfL.coefficients;

    // 2. Dynamic Mud Control: subtle -0.5 to -2.8 dB carve at 320 Hz to keep body clean
    float mudCutDb = -(0.5f + bodyVal * 2.3f);
    *mudDipL.coefficients = *Coeffs::makePeakFilter(currentSampleRate, 320.0f, 1.35f, juce::Decibels::decibelsToGain(mudCutDb));
    *mudDipR.coefficients = *mudDipL.coefficients;

    // 3. Color: Analog presence at 2.6 kHz (0 to +4.2 dB)
    float colorGainDb = colorVal * 4.2f;
    *colorPeakL.coefficients = *Coeffs::makePeakFilter(currentSampleRate, 2600.0f, 1.15f, juce::Decibels::decibelsToGain(colorGainDb));
    *colorPeakR.coefficients = *colorPeakL.coefficients;

    // 4. Freshness: Maag-style Air Band at 12.5 kHz (0 to +8.0 dB)
    float airGainDb = freshVal * 8.0f;
    *airShelfL.coefficients = *Coeffs::makeHighShelf(currentSampleRate, 12500.0f, 0.58f, juce::Decibels::decibelsToGain(airGainDb));
    *airShelfR.coefficients = *airShelfL.coefficients;
}

void MoonshineVoiceColorProcessor::startRelearn() {
    samplesAnalyzed = 0;
    accumLowEnergy  = 0.0;
    accumMudEnergy  = 0.0;
    accumMidEnergy  = 0.0;
    accumHighEnergy = 0.0;
    learnProgress.store(0.0f);
    optionsReady.store(false);
    activeProfileIndex.store(-1);
    learningActive.store(true);
}

void MoonshineVoiceColorProcessor::finalizeRelearnAnalysis() {
    learningActive.store(false);
    double total = accumLowEnergy + accumMudEnergy + accumMidEnergy + accumHighEnergy + 1e-9;

    float lowRatio  = static_cast<float>(accumLowEnergy / total);
    float mudRatio  = static_cast<float>(accumMudEnergy / total);
    float midRatio  = static_cast<float>(accumMidEnergy / total);
    float highRatio = static_cast<float>(accumHighEnergy / total);

    // Intelligent adaptive target curves for modern urban/pop vocals
    float baseBody  = juce::jlimit(25.0f, 82.0f, (0.38f - lowRatio + (mudRatio * 0.15f)) * 135.0f + 52.0f);
    float baseColor = juce::jlimit(30.0f, 85.0f, (0.35f - midRatio) * 115.0f + 56.0f);
    float baseFresh = juce::jlimit(32.0f, 92.0f, (0.22f - highRatio) * 165.0f + 62.0f);

    suggestedProfiles[0] = {
        "CRISP & FRESH", "Brillo & Aire Moderno",
        juce::jlimit(18.0f, 95.0f, baseBody - 10.0f),
        juce::jlimit(20.0f, 95.0f, baseColor + 4.0f),
        juce::jlimit(25.0f, 96.0f, baseFresh + 18.0f)
    };

    suggestedProfiles[1] = {
        "PRO BODY", "Peso & Presencia",
        juce::jlimit(25.0f, 96.0f, baseBody + 18.0f),
        juce::jlimit(20.0f, 95.0f, baseColor + 8.0f),
        juce::jlimit(18.0f, 95.0f, baseFresh - 8.0f)
    };

    suggestedProfiles[2] = {
        "STUDIO COLOR", "Balance Profesional",
        baseBody,
        baseColor,
        baseFresh
    };

    optionsReady.store(true);
    applyProfile(2); // Auto-apply balanced studio profile
}

void MoonshineVoiceColorProcessor::applyProfile(int index) {
    if (index < 0 || index >= 3) return;
    const auto& p = suggestedProfiles[index];
    activeProfileIndex.store(index);

    if (auto* param = apvts.getParameter("body"))
        param->setValueNotifyingHost(param->convertTo0to1(p.body));
    if (auto* param = apvts.getParameter("color"))
        param->setValueNotifyingHost(param->convertTo0to1(p.color));
    if (auto* param = apvts.getParameter("freshness"))
        param->setValueNotifyingHost(param->convertTo0to1(p.freshness));
}

void MoonshineVoiceColorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0) return;

    bool isBypassed = apvts.getRawParameterValue("bypass")->load() > 0.5f;

    float targetBody  = apvts.getRawParameterValue("body")->load() / 100.0f;
    float targetColor = apvts.getRawParameterValue("color")->load() / 100.0f;
    float targetFresh = apvts.getRawParameterValue("freshness")->load() / 100.0f;
    float targetMix   = apvts.getRawParameterValue("mix")->load() / 100.0f;
    float targetOut   = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load());

    smoothBody.setTargetValue(targetBody);
    smoothColor.setTargetValue(targetColor);
    smoothFresh.setTargetValue(targetFresh);
    smoothMix.setTargetValue(targetMix);
    smoothOut.setTargetValue(targetOut);

    updateDSPFilters(targetBody, targetColor, targetFresh);

    float inRmsSum  = 0.0f;
    float outRmsSum = 0.0f;
    float maxDeEss  = 0.0f;

    const float deEssAttack  = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.003f));
    const float deEssRelease = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.045f));
    const float glueAttack   = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.012f));
    const float glueRelease  = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * 0.120f));

    for (int sample = 0; sample < numSamples; ++sample) {
        float inL = buffer.getSample(0, sample);
        float inR = (numChannels > 1) ? buffer.getSample(1, sample) : inL;

        inRmsSum += (inL * inL + inR * inR) * 0.5f;

        // 1. Relearn analyzer (always listens when active, ignores silence)
        if (learningActive.load()) {
            float mono = 0.5f * (inL + inR);
            if (std::abs(mono) > 0.005f) {
                float l  = analyzeLow.processSample(mono);
                float mu = analyzeMud.processSample(mono);
                float m  = analyzeMid.processSample(mono);
                float h  = analyzeHigh.processSample(mono);

                accumLowEnergy  += static_cast<double>(l * l);
                accumMudEnergy  += static_cast<double>(mu * mu);
                accumMidEnergy  += static_cast<double>(m * m);
                accumHighEnergy += static_cast<double>(h * h);

                samplesAnalyzed++;
                learnProgress.store(static_cast<float>(samplesAnalyzed) / static_cast<float>(targetSamplesToAnalyze));

                if (samplesAnalyzed >= targetSamplesToAnalyze) {
                    finalizeRelearnAnalysis();
                }
            }
        }

        float bodyAmt  = smoothBody.getNextValue();
        float colorAmt = smoothColor.getNextValue();
        float freshAmt = smoothFresh.getNextValue();
        float mixAmt   = smoothMix.getNextValue();
        float outGain  = smoothOut.getNextValue();

        if (isBypassed) {
            outRmsSum += (inL * inL + inR * inR) * 0.5f;
            continue;
        }

        // 2. Subsonic cleanup + Dynamic mud carve + Vocal Body
        float wetL = hpCleanL.processSample(inL);
        float wetR = hpCleanR.processSample(inR);

        wetL = mudDipL.processSample(wetL);
        wetR = mudDipR.processSample(wetR);

        wetL = bodyShelfL.processSample(wetL);
        wetR = bodyShelfR.processSample(wetR);

        // 3. Analog Color: Asymmetric Tube/Tape harmonic saturation + console presence
        float drive = 1.0f + (colorAmt * 2.8f);
        float bias  = colorAmt * 0.06f; // Generates warm 2nd-order even harmonics
        float norm  = 1.0f / std::tanh(drive);
        wetL = (std::tanh((wetL + bias) * drive) - std::tanh(bias * drive)) * norm;
        wetR = (std::tanh((wetR + bias) * drive) - std::tanh(bias * drive)) * norm;

        wetL = colorPeakL.processSample(wetL);
        wetR = colorPeakR.processSample(wetR);

        // 4. Freshness: Air Shelf + High-Frequency Harmonic Exciter
        float airL = airShelfL.processSample(wetL);
        float airR = airShelfR.processSample(wetR);

        float excL = (airL - wetL) * (0.20f * freshAmt);
        float excR = (airR - wetR) * (0.20f * freshAmt);
        wetL = airL + std::tanh(excL * 2.8f) * 0.28f;
        wetR = airR + std::tanh(excR * 2.8f) * 0.28f;

        // 5. Built-in Smooth De-Esser (protects freshness from harsh sibilance)
        float sibL = sibilanceDetectL.processSample(wetL);
        float sibR = sibilanceDetectR.processSample(wetR);
        float sibLevel = std::max(std::abs(sibL), std::abs(sibR));

        if (sibLevel > deEssEnv)
            deEssEnv = deEssAttack * deEssEnv + (1.0f - deEssAttack) * sibLevel;
        else
            deEssEnv = deEssRelease * deEssEnv + (1.0f - deEssRelease) * sibLevel;

        float deEssThresh = 0.08f;
        float deEssReduction = 0.0f;
        if (deEssEnv > deEssThresh && freshAmt > 0.1f) {
            float over = (deEssEnv - deEssThresh) / (deEssEnv + 1e-6f);
            deEssReduction = juce::jlimit(0.0f, 0.45f, over * freshAmt * 0.65f);
            wetL -= sibL * deEssReduction;
            wetR -= sibR * deEssReduction;
        }
        maxDeEss = std::max(maxDeEss, deEssReduction);

        // 6. Subtle Studio Vocal Glue (gentle opto-style leveling)
        float peak = std::max(std::abs(wetL), std::abs(wetR));
        if (peak > glueEnv)
            glueEnv = glueAttack * glueEnv + (1.0f - glueAttack) * peak;
        else
            glueEnv = glueRelease * glueEnv + (1.0f - glueRelease) * peak;

        float glueGain = 1.0f;
        if (glueEnv > 0.35f) {
            float ratio = 1.0f + (bodyAmt * 0.5f + colorAmt * 0.5f);
            float overDb = juce::Decibels::gainToDecibels(glueEnv / 0.35f);
            float grDb = overDb * (1.0f - 1.0f / ratio) * 0.45f;
            glueGain = juce::Decibels::decibelsToGain(-grDb);
        }

        // Auto-gain compensation so turning knobs keeps perceived loudness balanced
        float autoComp = 1.0f / (1.0f + (bodyAmt * 0.12f) + (colorAmt * 0.10f) + (freshAmt * 0.10f));
        wetL *= glueGain * autoComp;
        wetR *= glueGain * autoComp;

        // 7. Dry/Wet Mix & Output Gain + Soft Safety Limiter
        float finalL = ((inL * (1.0f - mixAmt)) + (wetL * mixAmt)) * outGain;
        float finalR = ((inR * (1.0f - mixAmt)) + (wetR * mixAmt)) * outGain;

        finalL = std::tanh(finalL * 0.95f) / 0.95f;
        finalR = std::tanh(finalR * 0.95f) / 0.95f;

        outRmsSum += (finalL * finalL + finalR * finalR) * 0.5f;

        buffer.setSample(0, sample, finalL);
        if (numChannels > 1) buffer.setSample(1, sample, finalR);
    }

    currentInputRms.store(std::sqrt(inRmsSum / static_cast<float>(numSamples)));
    currentOutputRms.store(std::sqrt(outRmsSum / static_cast<float>(numSamples)));
    currentDeEssGr.store(maxDeEss);
}

void MoonshineVoiceColorProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MoonshineVoiceColorProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* MoonshineVoiceColorProcessor::createEditor() {
    return new MoonshineVoiceColorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MoonshineVoiceColorProcessor();
}
