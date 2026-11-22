#pragma once
#include "PluginProcessor.h"

class MoonshineLookAndFeel : public juce::LookAndFeel_V4 {
public:
    MoonshineLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
};

class MoonshineVoiceColorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MoonshineVoiceColorEditor(MoonshineVoiceColorProcessor&);
    ~MoonshineVoiceColorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text,
                   juce::Colour accent, const juce::String& suffix);

    MoonshineVoiceColorProcessor& audioProcessor;
    MoonshineLookAndFeel customLookAndFeel;

    juce::Slider bodySlider, colorSlider, freshnessSlider, mixSlider, outputSlider;
    juce::Label bodyLabel, colorLabel, freshnessLabel, mixLabel, outputLabel;

    juce::TextButton relearnButton { "RELEARN VOICE" };
    juce::TextButton bypassButton  { "ACTIVE" };
    juce::TextButton optionButtons[3];

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAtt> bodyAtt, colorAtt, freshnessAtt, mixAtt, outputAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;

    float smoothedInLevel = 0.0f;
    float smoothedOutLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoonshineVoiceColorEditor)
};
