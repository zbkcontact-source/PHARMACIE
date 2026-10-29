#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "DSPUtils.h"

namespace Mods
{
    enum Id { weed = 0, coke, keta, lsd, mdma, count };
    inline const char* key (int i)   { static const char* k[] = { "weed", "coke", "keta", "lsd", "mdma" }; return k[i]; }
    inline const char* label (int i) { static const char* n[] = { "WEED", "COKE", "KETA", "LSD", "MDMA" }; return n[i]; }
    inline const char* tool (int i)  { static const char* n[] = { "TAPE", "PUNCH", "GRAIN", "COLOR", "ENRICH" }; return n[i]; }

    // deux réglages avancés par module
    inline const char* advName (int i, int which)
    {
        static const char* a[] = { "SPEED", "ATTACK", "SIZE", "RATE", "TONE" };
        static const char* b[] = { "HISS", "SUSTAIN", "SPRAY", "DEPTH", "PULSE" };
        return which == 0 ? a[i] : b[i];
    }
    inline float advDefault (int i, int which)
    {
        static const float a[] = { 50.0f, 30.0f, 35.0f, 30.0f, 50.0f };
        static const float b[] = { 20.0f, 50.0f, 40.0f, 50.0f, 0.0f };
        return which == 0 ? a[i] : b[i];
    }
    inline juce::String advText (int i, int which, float v)
    {
        if (which == 0)
            switch (i)
            {
                case 0: return juce::String (7.5f * std::pow (4.0f, v), 1) + " ips";
                case 1: return juce::String (juce::roundToInt (5.0f + 45.0f * v)) + " ms";
                case 2: return juce::String (juce::roundToInt (20.0f + 400.0f * v)) + " ms";
                case 3: return juce::String (0.1f * std::pow (60.0f, v), 2) + " Hz";
                default: return v < 0.4f ? "WARM" : (v > 0.6f ? "BRIGHT" : "EVEN");
            }
        switch (i)
        {
            case 1:
            {
                const int s = juce::roundToInt ((v - 0.5f) * 200.0f);
                return s == 0 ? juce::String ("0") : (s > 0 ? "+" : "") + juce::String (s);
            }
            case 2: return juce::String (juce::roundToInt (v * 1500.0f)) + " ms";
            default: return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        }
    }
}

struct Env
{
    float v = 0.0f, a = 0.0f, r = 0.0f;
    void set (double sr, float atkSec, float relSec)
    {
        a = 1.0f - std::exp (-1.0f / (atkSec * (float) sr));
        r = 1.0f - std::exp (-1.0f / (relSec * (float) sr));
    }
    float process (float x) { v += (x > v ? a : r) * (x - v); return v; }
};

class PharmacieAudioProcessor : public juce::AudioProcessor
{
public:
    PharmacieAudioProcessor();
    ~PharmacieAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PHARMACY"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    std::array<std::atomic<float>, Mods::count> effectLevel {};
    std::atomic<float> outputLevel { 0.0f }, cleanLevel { 0.0f };
    std::atomic<float> beatPhase { 0.0f };

private:
    float doseOf (int m) const;
    float param (int m, const char* suffix) const;
    void processChain (float* L, float* R, int n, double fs, double beatStart, double beatInc);

    double sr = 44100.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    std::array<dsp2::Smooth, Mods::count> sDose, sMix, sA1, sA2;
    dsp2::Smooth sGlobalMix, sOut, sClean;
    float levelSmooth[Mods::count] {};
    float cleanSmooth = 0.0f;
    juce::Random rng;
    double freeBeat = 0.0;
    bool prepared = false;

    // WEED : bande
    std::array<dsp2::SVF, 2> tapeBump, tapeLP1, tapeLP2, hissHP, preEm, deEm;
    Env tapeEnv;
    // COKE : transitoires
    Env envFast, envSlow, envSus, envLong;
    std::array<dsp2::SVF, 2> presence;
    // KETA : granulaire
    struct Grain { bool active = false; long long start = 0; int len = 1, age = 0; float gl = 1, gr = 1; bool rev = false; };
    std::array<Grain, 48> grains;
    std::array<dsp2::Delay, 2> grainBuf;
    long long recIndex = 0;
    double grainAcc = 0.0;
    // LSD : couleur
    std::array<dsp2::Delay, 2> chorusBuf;
    std::array<std::array<dsp2::Allpass1, 6>, 2> phaser;
    float phFb[2] {};
    double lfoPh = 0.0;
    // MDMA : enrichissement + pulse
    std::array<dsp2::SVF, 2> xLow, xHigh, subBP, exHP, airHP;
    std::array<Env, 3> bandEnv;
    Env glueEnv;
    // CLEAN
    std::array<dsp2::SVF, 2> harshBP, warmPk, softHi;
    Env harshEnv, fullEnv, levEnv;
    // DC
    float dcX[2] {}, dcY[2] {};

    juce::Reverb coldVerb;
    juce::AudioBuffer<float> coldSend, dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PharmacieAudioProcessor)
};
