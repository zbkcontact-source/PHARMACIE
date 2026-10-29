#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour bg     { 0xff101418 };
    const juce::Colour panel  { 0xff171c22 };
    const juce::Colour panel2 { 0xff20262e };
    const juce::Colour line   { 0xff2c343e };
    const juce::Colour fg     { 0xffeef3f0 };
    const juce::Colour muted  { 0xff8c99a3 };
    const juce::Colour screen { 0xff0a0d10 };
    const juce::Colour neon   { 0xff5dff8a };

    inline juce::Colour mod (int i)
    {
        static const juce::Colour c[] = {
            juce::Colour (0xff7fdc8c),   // WEED vert
            juce::Colour (0xff8fd3ff),   // COKE bleu glacé
            juce::Colour (0xffdfe4ee),   // KETA argent
            juce::Colour (0xffc084fc),   // LSD violet
            juce::Colour (0xffff7eb9) }; // MDMA rose
        return c[i];
    }
}

class PharmacieLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PharmacieLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
// L'écran : la boutique, le pharmacien et son visage animé selon les doses
class Visualizer : public juce::Component
{
public:
    Visualizer();
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (const float doses[Mods::count], const float levels[Mods::count], float outLevel, float beat, float clean);

private:
    struct Particle
    {
        float x, y, vx, vy, life, maxLife, size, rot, vr, hue;
        int type;
    };

    void spawn (int type, float x, float y, float vx, float vy, float life, float size, float hue = 0.0f);
    void drawFace (juce::Graphics&);
    void drawEye (juce::Graphics&, float cx, float cy, float rx, float ry, float lidFrac, float lookX, int side);
    void drawMouth (juce::Graphics&);
    void drawNeon (juce::Graphics&);
    void drawOverlays (juce::Graphics&);
    juce::Point<float> P (float ix, float iy) const { return { ix * k, iy * k }; }

    juce::Image shop;
    juce::Path neonText, neonCrossL, neonCrossR;
    std::vector<Particle> parts;
    juce::Random rnd;

    float d[Mods::count] {}, lv[Mods::count] {};
    float cl = 0.0f;
    float level = 0.0f, beatPh = 0.0f, t = 0.0f, k = 0.3125f;
    float lookX = 0.0f, lookTarget = 0.0f, lookTimer = 0.0f;
    float shakeX = 0.0f, shakeY = 0.0f;
    float neonAlpha = 1.0f;
    float spawnAcc[8] {};
};

//==============================================================================
class ModuleStrip : public juce::Component
{
public:
    ModuleStrip (juce::AudioProcessorValueTreeState&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void update (float effectLevel);

private:
    int index;
    juce::Colour colour;
    juce::ToggleButton power;
    juce::Slider dose, mix, adv, adv2;
    juce::AudioProcessorValueTreeState::ButtonAttachment powerAtt;
    juce::AudioProcessorValueTreeState::SliderAttachment doseAtt, mixAtt, advAtt, adv2Att;
    float level = 0.0f;
    juce::String feeling;
};

//==============================================================================
class PharmacieEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PharmacieEditor (PharmacieAudioProcessor&);
    ~PharmacieEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 960, baseH = 876;

private:
    void timerCallback() override;

    PharmacieAudioProcessor& proc;
    PharmacieLookAndFeel lnf;

    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
    } content;

    Visualizer visual;
    std::array<std::unique_ptr<ModuleStrip>, Mods::count> strips;
    juce::Slider mixSlider, outSlider, cleanSlider;
    juce::Label mixLabel, outLabel;
    juce::ToggleButton cleanBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, outAtt, cleanAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cleanBtnAtt;
    float visClean = 0.0f;
    float visDose[Mods::count] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PharmacieEditor)
};
