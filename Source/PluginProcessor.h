#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// Filtre à variable d'état (topologie TPT), sorties passe-bas et passe-haut
struct SVF
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float lp = 0.0f, hp = 0.0f;

    void reset() { ic1 = ic2 = lp = hp = 0.0f; }

    void process (float v0, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        hp = v0 - k * v1 - v2;
    }
};

// Lissage exponentiel d'un paramètre, échantillon par échantillon
struct Smooth
{
    float value = 0.0f, target = 0.0f, coeff = 0.001f;
    void setTime (double sr, double seconds) { coeff = (float) (1.0 - std::exp (-1.0 / (seconds * sr))); }
    void snap (float v) { value = target = v; }
    float next() { value += coeff * (target - value); return value; }
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

    const juce::String getName() const override { return "PHARMACIE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Niveau de sortie pour l'animation de l'interface
    std::atomic<float> outputLevel { 0.0f };

private:
    float dose (const char* id) const;

    double sr = 44100.0;

    // Lissages : dose effective de chaque module + mix/sortie
    Smooth sLsd, sCoke, sWeed, sAcid, sMix, sOut;

    // Pitch (cocaïne accélère, weed ralentit)
    std::array<std::vector<float>, 2> pitchBuf;
    int pitchWrite = 0;
    double pitchPhase = 0.0, wowPhase = 0.0;

    // Weed : passe-bas
    std::array<SVF, 2> weedLP;
    // Cocaïne : aigus, compression, trémolo
    std::array<SVF, 2> cokeHP;
    float compEnv = 0.0f;
    double tremPhase = 0.0;
    float tremSmooth = 1.0f;
    // Acide : filtre résonant, saturation, anneau
    std::array<SVF, 2> acidLP;
    double acidPhase = 0.0, ringPhase = 0.0;
    // LSD : flanger/chorus + panoramique
    std::array<std::vector<float>, 2> lsdBuf;
    int lsdWrite = 0;
    double lsdPhase = 0.0, panPhase = 0.0;
    // Réverb partagée
    juce::Reverb reverb;
    juce::AudioBuffer<float> sendBuf, dryBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PharmacieAudioProcessor)
};
