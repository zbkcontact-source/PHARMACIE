#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour bg     { 0xff121019 };
    const juce::Colour panel  { 0xff1b1824 };
    const juce::Colour panel2 { 0xff24202f };
    const juce::Colour line   { 0xff342e43 };
    const juce::Colour fg     { 0xffefeaf7 };
    const juce::Colour muted  { 0xff9d96b0 };
    const juce::Colour screen { 0xff09080e };
    const juce::Colour lsd    { 0xffc084fc };
    const juce::Colour coke   { 0xff8fd3ff };
    const juce::Colour weed   { 0xff7ccf8a };
    const juce::Colour acid   { 0xffffd23f };
}

class PharmacieLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PharmacieLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name,
                 juce::Colour colour, const juce::String& description, juce::StringArray moods);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    float effectiveDose() const;
    juce::Colour colour;

private:
    juce::String name, description;
    juce::StringArray moods;
    juce::ToggleButton power;
    juce::Slider knob;
    juce::Label mood;
    juce::AudioProcessorValueTreeState::ButtonAttachment powerAtt;
    juce::AudioProcessorValueTreeState::SliderAttachment knobAtt;
    bool lastOn = false;
};

class PharmacieEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PharmacieEditor (PharmacieAudioProcessor&);
    ~PharmacieEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintVisual (juce::Graphics&, juce::Rectangle<float>);

    PharmacieAudioProcessor& proc;
    PharmacieLookAndFeel lnf;
    std::array<std::unique_ptr<ModulePanel>, 4> modules;
    juce::Slider mixSlider, outSlider;
    juce::Label mixLabel, outLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, outAtt;
    juce::Rectangle<int> visualArea;
    double t = 0.0;
    float level = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PharmacieEditor)
};
