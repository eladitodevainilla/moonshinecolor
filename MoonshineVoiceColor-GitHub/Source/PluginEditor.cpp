#include "PluginEditor.h"

MoonshineLookAndFeel::MoonshineLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void MoonshineLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, const float rotaryStartAngle,
                                            const float rotaryEndAngle, juce::Slider& slider) {
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(8.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = (width > 110) ? 6.0f : 4.0f;
    auto arcRadius = radius - lineW * 0.5f;

    auto accentColour = slider.findColour(juce::Slider::rotarySliderFillColourId);

    // Track background
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff242838));
    g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active neon glow + arc
    if (slider.isEnabled()) {
        juce::Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius,
                               0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(accentColour.withAlpha(0.22f));
        g.strokePath(valueArc, juce::PathStrokeType(lineW + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(accentColour);
        g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Metallic dark knob cap
    auto knobBounds = bounds.reduced(width > 110 ? 15.0f : 10.0f);
    juce::ColourGradient knobGrad(juce::Colour(0xff232736), knobBounds.getCentreX(), knobBounds.getY(),
                                  juce::Colour(0xff12141c), knobBounds.getCentreX(), knobBounds.getBottom(), false);
    g.setGradientFill(knobGrad);
    g.fillEllipse(knobBounds);

    g.setColour(juce::Colour(0xff383e54));
    g.drawEllipse(knobBounds, 1.2f);

    // Pointer indicator
    juce::Path p;
    auto pointerLength = knobBounds.getWidth() * 0.30f;
    auto pointerThickness = (width > 110) ? 3.2f : 2.2f;
    p.addRoundedRectangle(-pointerThickness * 0.5f, -knobBounds.getWidth() * 0.43f, pointerThickness, pointerLength, 1.5f);
    p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(bounds.getCentreX(), bounds.getCentreY()));
    g.setColour(juce::Colours::white);
    g.fillPath(p);
}

void MoonshineLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto base = backgroundColour;
    if (shouldDrawButtonAsDown) base = base.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted) base = base.brighter(0.1f);

    g.setColour(base);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(button.findColour(juce::TextButton::textColourOffId).withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.2f);
}

MoonshineVoiceColorEditor::MoonshineVoiceColorEditor(MoonshineVoiceColorProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
    setLookAndFeel(&customLookAndFeel);

    setupKnob(bodySlider, bodyLabel, "BODY", juce::Colour(0xffff6b4a), "%");
    setupKnob(colorSlider, colorLabel, "COLOR", juce::Colour(0xffb86bff), "%");
    setupKnob(freshnessSlider, freshnessLabel, "FRESHNESS", juce::Colour(0xff00e5ff), "%");

    setupKnob(mixSlider, mixLabel, "MIX", juce::Colour(0xff94a3b8), "%");
    setupKnob(outputSlider, outputLabel, "OUTPUT", juce::Colour(0xff94a3b8), " dB");

    bodyAtt      = std::make_unique<SliderAtt>(audioProcessor.apvts, "body", bodySlider);
    colorAtt     = std::make_unique<SliderAtt>(audioProcessor.apvts, "color", colorSlider);
    freshnessAtt = std::make_unique<SliderAtt>(audioProcessor.apvts, "freshness", freshnessSlider);
    mixAtt       = std::make_unique<SliderAtt>(audioProcessor.apvts, "mix", mixSlider);
    outputAtt    = std::make_unique<SliderAtt>(audioProcessor.apvts, "output", outputSlider);

    // Bypass button
    bypassButton.setClickingTogglesState(true);
    bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffef4444).withAlpha(0.25f));
    bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff38bdf8));
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xfff87171));
    bypassAtt = std::make_unique<ButtonAtt>(audioProcessor.apvts, "bypass", bypassButton);
    addAndMakeVisible(bypassButton);

    // Relearn button
    relearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00e5ff).withAlpha(0.16f));
    relearnButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
    relearnButton.onClick = [this]() { audioProcessor.startRelearn(); };
    addAndMakeVisible(relearnButton);

    // 3 AI Profile buttons
    for (int i = 0; i < 3; ++i) {
        optionButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b1f2e));
        optionButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        optionButtons[i].onClick = [this, i]() {
            audioProcessor.applyProfile(i);
        };
        addAndMakeVisible(optionButtons[i]);
    }

    setSize(720, 440);
    startTimerHz(30);
}

MoonshineVoiceColorEditor::~MoonshineVoiceColorEditor() {
    setLookAndFeel(nullptr);
}

void MoonshineVoiceColorEditor::setupKnob(juce::Slider& slider, juce::Label& label,
                                          const juce::String& text, juce::Colour accent,
                                          const juce::String& suffix) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 20);
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(0);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    addAndMakeVisible(label);
}

void MoonshineVoiceColorEditor::timerCallback() {
    bool isBypassed = bypassButton.getToggleState();
    bypassButton.setButtonText(isBypassed ? "BYPASSED" : "ACTIVE");

    if (audioProcessor.isLearning()) {
        int pct = static_cast<int>(audioProcessor.getLearnProgress() * 100.0f);
        relearnButton.setButtonText("LISTENING... " + juce::String(pct) + "%");
        relearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff6b4a).withAlpha(0.28f));
        relearnButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff6b4a));
    } else {
        relearnButton.setButtonText("RELEARN VOICE");
        relearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00e5ff).withAlpha(0.16f));
        relearnButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
    }

    auto profiles = audioProcessor.getSuggestedProfiles();
    int activeIdx = audioProcessor.getActiveProfileIndex();
    for (int i = 0; i < 3; ++i) {
        optionButtons[i].setButtonText(profiles[i].name + "\n" + profiles[i].subtitle);
        if (i == activeIdx) {
            optionButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00e5ff).withAlpha(0.20f));
            optionButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00e5ff));
        } else {
            optionButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b1f2e));
            optionButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        }
    }

    float inTarget  = juce::jlimit(0.0f, 1.0f, audioProcessor.getInputLevel() * 4.5f);
    float outTarget = juce::jlimit(0.0f, 1.0f, audioProcessor.getOutputLevel() * 4.5f);
    smoothedInLevel  = smoothedInLevel * 0.72f + inTarget * 0.28f;
    smoothedOutLevel = smoothedOutLevel * 0.72f + outTarget * 0.28f;

    repaint();
}

void MoonshineVoiceColorEditor::paint(juce::Graphics& g) {
    // Deep studio obsidian gradient
    juce::ColourGradient bg(juce::Colour(0xff0b0d13), 0, 0,
                            juce::Colour(0xff151824), 0, static_cast<float>(getHeight()), false);
    g.setGradientFill(bg);
    g.fillAll();

    // Header Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("MOONSHINE VOICE COLOR", 24, 18, 350, 28, juce::Justification::left);

    g.setColour(juce::Colour(0xff64748b));
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("ALR WORLDWIDE  |  HARMONIC & AIR VOCAL ENGINE", 24, 46, 360, 16, juce::Justification::left);

    // Live Dual Input/Output Vocal Meter
    float meterX = 24.0f;
    float meterW = static_cast<float>(getWidth() - 48);
    g.setColour(juce::Colour(0xff1c2030));
    g.fillRoundedRectangle(meterX, 72.0f, meterW, 6.0f, 3.0f);

    juce::ColourGradient meterGrad(juce::Colour(0xffff6b4a), meterX, 72.0f,
                                   juce::Colour(0xff00e5ff), meterX + meterW, 72.0f, false);
    g.setGradientFill(meterGrad);
    g.fillRoundedRectangle(meterX, 72.0f, meterW * smoothedOutLevel, 6.0f, 3.0f);

    // Main DSP Panel
    g.setColour(juce::Colour(0xff121520));
    g.fillRoundedRectangle(20.0f, 92.0f, static_cast<float>(getWidth() - 40), 220.0f, 14.0f);
    g.setColour(juce::Colour(0xff23283c));
    g.drawRoundedRectangle(20.0f, 92.0f, static_cast<float>(getWidth() - 40), 220.0f, 14.0f, 1.2f);

    // Vertical divider before Mix/Output
    g.setColour(juce::Colour(0xff23283c));
    g.drawLine(static_cast<float>(getWidth() - 148), 108.0f, static_cast<float>(getWidth() - 148), 296.0f, 1.0f);

    // Footer Section Label
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("SMART VOICE PROFILES (PRESS RELEARN & PLAY VOCAL):", 24, 328, 420, 18, juce::Justification::left);

    // De-Esser active indicator
    float deEss = audioProcessor.getDeEsserReduction();
    if (deEss > 0.02f) {
        g.setColour(juce::Colour(0xff00e5ff));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("SMOOTH DE-ESS ACTIVE", getWidth() - 180, 328, 156, 18, juce::Justification::right);
    }
}

void MoonshineVoiceColorEditor::resized() {
    relearnButton.setBounds(getWidth() - 184, 20, 160, 38);
    bypassButton.setBounds(getWidth() - 280, 20, 86, 38);

    int mainKnobW = 152;
    int mainKnobH = 152;
    int startX = 40;
    int knobY = 130;

    bodyLabel.setBounds(startX, 104, mainKnobW, 20);
    bodySlider.setBounds(startX, knobY, mainKnobW, mainKnobH);

    colorLabel.setBounds(startX + 174, 104, mainKnobW, 20);
    colorSlider.setBounds(startX + 174, knobY, mainKnobW, mainKnobH);

    freshnessLabel.setBounds(startX + 348, 104, mainKnobW, 20);
    freshnessSlider.setBounds(startX + 348, knobY, mainKnobW, mainKnobH);

    int smallX = getWidth() - 126;
    mixLabel.setBounds(smallX, 102, 92, 16);
    mixSlider.setBounds(smallX, 118, 92, 86);

    outputLabel.setBounds(smallX, 208, 92, 16);
    outputSlider.setBounds(smallX, 224, 92, 86);

    int btnW = (getWidth() - 68) / 3;
    for (int i = 0; i < 3; ++i) {
        optionButtons[i].setBounds(24 + i * (btnW + 10), 352, btnW, 62);
    }
}
