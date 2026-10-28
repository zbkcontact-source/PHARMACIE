#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr int bufSize = 1 << 16;
    constexpr int bufMask = bufSize - 1;

    inline void wrap (double& p) { p -= std::floor (p); }

    // Lecture interpolée à "delay" échantillons derrière l'index d'écriture
    inline float readDelay (const std::vector<float>& b, int write, float delay)
    {
        const double rp = (double) write - (double) delay + (double) bufSize;
        const int i0 = (int) rp;
        const float frac = (float) (rp - (double) i0);
        const float a = b[(size_t) (i0 & bufMask)];
        const float c = b[(size_t) ((i0 + 1) & bufMask)];
        return a + frac * (c - a);
    }

    inline float softClip (float x)
    {
        const float ax = std::abs (x);
        if (ax < 1.0f) return x;   // transparent sous 0 dBFS, garde-fou au-dessus
        return std::copysign (1.0f + 0.5f * std::tanh ((ax - 1.0f) / 0.5f), x);
    }

    juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
}

PharmacieAudioProcessor::PharmacieAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PharmacieAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const std::array<std::pair<const char*, const char*>, 4> mods {{
        { "lsd", "LSD" }, { "coke", "Cocaine" }, { "weed", "Weed" }, { "acid", "Acide" } }};

    for (auto& [id, name] : mods)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { juce::String (id) + "_on", 1 }, juce::String (name) + " On", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { juce::String (id) + "_dose", 1 }, juce::String (name) + " Dose",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "out", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " dB"; })));

    return layout;
}

float PharmacieAudioProcessor::dose (const char* id) const
{
    const auto base = juce::String (id);
    const bool on = apvts.getRawParameterValue (base + "_on")->load() > 0.5f;
    return on ? apvts.getRawParameterValue (base + "_dose")->load() / 100.0f : 0.0f;
}

bool PharmacieAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PharmacieAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    for (auto* s : { &sLsd, &sCoke, &sWeed, &sAcid })
    {
        s->setTime (sr, 0.05);
        s->snap (0.0f);
    }
    sMix.setTime (sr, 0.03);
    sMix.snap (apvts.getRawParameterValue ("mix")->load() / 100.0f);
    sOut.setTime (sr, 0.03);
    sOut.snap (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));

    for (auto& b : pitchBuf) b.assign (bufSize, 0.0f);
    for (auto& b : lsdBuf) b.assign (bufSize, 0.0f);
    pitchWrite = lsdWrite = 0;
    pitchPhase = wowPhase = tremPhase = acidPhase = ringPhase = lsdPhase = panPhase = 0.0;
    compEnv = 0.0f;
    tremSmooth = 1.0f;
    for (auto* f : { &weedLP, &cokeHP, &acidLP })
        for (auto& s : *f) s.reset();

    reverb.setSampleRate (sr);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.9f;
    rp.damping = 0.45f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.reset();

    sendBuf.setSize (2, samplesPerBlock);
    dryBuf.setSize (2, samplesPerBlock);
}

void PharmacieAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (n == 0 || nch == 0) return;

    sLsd.target = dose ("lsd");
    sCoke.target = dose ("coke");
    sWeed.target = dose ("weed");
    sAcid.target = dose ("acid");
    sMix.target = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    sOut.target = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load());

    sendBuf.setSize (2, n, false, false, true);
    dryBuf.setSize (2, n, false, false, true);

    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;
    float* sL = sendBuf.getWritePointer (0);
    float* sR = sendBuf.getWritePointer (1);
    float* dL = dryBuf.getWritePointer (0);
    float* dR = dryBuf.getWritePointer (1);

    const float fsr = (float) sr;
    const float nyqSafe = 0.45f * fsr;
    const float pitchWin = 0.07f * fsr;
    const float tremCoef = 1.0f - std::exp (-1.0f / (0.002f * fsr));
    const float atk = 1.0f - std::exp (-1.0f / (0.0005f * fsr));
    const float rel = 1.0f - std::exp (-1.0f / (0.12f * fsr));
    const float gCoke = std::tan (pi * 3500.0f / fsr);

    for (int i = 0; i < n; ++i)
    {
        float l = L[i];
        float r = R ? R[i] : l;
        dL[i] = l;
        dR[i] = r;

        const float Ld = sLsd.next(), C = sCoke.next(), W = sWeed.next(), A = sAcid.next();

        // --- Vitesse : la cocaïne accélère (pitch +), la weed ralentit (pitch −) avec pleurage de bande
        pitchBuf[0][(size_t) pitchWrite] = l;
        pitchBuf[1][(size_t) pitchWrite] = r;
        const float pAmt = juce::jmin (1.0f, (C + W) * 8.0f);
        if (pAmt > 0.0001f)
        {
            wowPhase += (0.35 + W * 0.6) / sr;  wrap (wowPhase);
            const float ratio = 1.0f + C * 0.25f - W * 0.25f + W * 0.03f * std::sin (twoPi * (float) wowPhase);
            pitchPhase += (1.0 - ratio) / pitchWin;  wrap (pitchPhase);
            double ph2 = pitchPhase + 0.5;  wrap (ph2);
            const float d1 = (float) pitchPhase * pitchWin + 2.0f;
            const float d2 = (float) ph2 * pitchWin + 2.0f;
            const float s1 = std::sin (pi * (float) pitchPhase), s2 = std::sin (pi * (float) ph2);
            const float g1 = s1 * s1, g2 = s2 * s2;
            const float pl = g1 * readDelay (pitchBuf[0], pitchWrite, d1) + g2 * readDelay (pitchBuf[0], pitchWrite, d2);
            const float pr = g1 * readDelay (pitchBuf[1], pitchWrite, d1) + g2 * readDelay (pitchBuf[1], pitchWrite, d2);
            l += pAmt * (pl - l);
            r += pAmt * (pr - r);
        }
        pitchWrite = (pitchWrite + 1) & bufMask;

        // --- Weed : son étouffé
        if (W > 0.0001f)
        {
            const float fc = juce::jmin (nyqSafe, 20000.0f * std::pow (420.0f / 20000.0f, W));
            const float g = std::tan (pi * fc / fsr);
            weedLP[0].process (l, g, 1.2f);  l = weedLP[0].lp;
            weedLP[1].process (r, g, 1.2f);  r = weedLP[1].lp;
        }

        // --- Cocaïne : aigus agressifs, compression écrasée, trémolo nerveux
        if (C > 0.0001f)
        {
            const float shelf = juce::Decibels::decibelsToGain (C * 14.0f) - 1.0f;
            cokeHP[0].process (l, gCoke, 1.414f);  l += shelf * cokeHP[0].hp;
            cokeHP[1].process (r, gCoke, 1.414f);  r += shelf * cokeHP[1].hp;

            const float level = juce::jmax (std::abs (l), std::abs (r));
            compEnv += (level > compEnv ? atk : rel) * (level - compEnv);
            const float envDb = juce::Decibels::gainToDecibels (compEnv, -100.0f);
            const float thr = -C * 30.0f;
            const float over = envDb - thr;
            const float ratio = 1.0f + C * 11.0f;
            const float grDb = over > 0.0f ? -over * (1.0f - 1.0f / ratio) : 0.0f;
            const float makeupDb = -thr * (1.0f - 1.0f / ratio) * 0.9f;
            const float cg = juce::Decibels::decibelsToGain (grDb + makeupDb);
            l *= cg;  r *= cg;

            tremPhase += (6.0 + C * 14.0) / sr;  wrap (tremPhase);
            tremSmooth += tremCoef * ((tremPhase < 0.5 ? 1.0f : -1.0f) - tremSmooth);
            const float td = C * 0.42f;
            const float tg = 1.0f - td + td * tremSmooth;
            l *= tg;  r *= tg;
        }

        // --- Acide : filtre résonant qui squelche, saturation, grain numérique, modulation en anneau
        if (A > 0.0001f)
        {
            acidPhase += (0.3 + A * 3.6) / sr;  wrap (acidPhase);
            ringPhase += (40.0 + A * 260.0) / sr;  wrap (ringPhase);
            const float base = 350.0f + (1.0f - A) * 1800.0f;
            const float fc = juce::jlimit (40.0f, nyqSafe, base + base * 0.85f * std::sin (twoPi * (float) acidPhase));
            const float g = std::tan (pi * fc / fsr);
            const float k = 1.0f / (0.7f + A * 14.0f);
            const float drive = 1.0f + A * 22.0f;
            const float outScale = 1.4f / std::sqrt (drive);
            const float levels = std::pow (2.0f, 12.0f - A * 9.0f);
            const float rd = A * 0.35f;
            const float rg = (1.0f - rd + rd * std::sin (twoPi * (float) ringPhase)) * outScale;
            const float amt = juce::jmin (1.0f, A * 8.0f);

            auto acid = [&] (SVF& f, float x)
            {
                f.process (x, g, k);
                const float sh = std::tanh (f.lp * drive);
                return std::round (sh * levels) / levels * rg;
            };
            l += amt * (acid (acidLP[0], l) - l);
            r += amt * (acid (acidLP[1], r) - r);
        }

        // --- LSD : chorus/flanger flottant + panoramique qui tourne
        if (Ld > 0.0001f)
        {
            lsdPhase += (0.15 + Ld * 1.8) / sr;  wrap (lsdPhase);
            panPhase += (0.1 + Ld * 0.6) / sr;  wrap (panPhase);
            const float base = 0.006f + Ld * 0.012f;
            const float depth = 0.0015f + Ld * 0.0065f;
            const float dl = (base + depth * std::sin (twoPi * (float) lsdPhase)) * fsr;
            const float dr = (base + depth * std::sin (twoPi * (float) lsdPhase + pi * 0.5f)) * fsr;
            const float yl = readDelay (lsdBuf[0], lsdWrite, dl);
            const float yr = readDelay (lsdBuf[1], lsdWrite, dr);
            const float fb = Ld * 0.6f, wet = Ld * 0.7f, dry = 1.0f - Ld * 0.35f;
            lsdBuf[0][(size_t) lsdWrite] = l + fb * yl;
            lsdBuf[1][(size_t) lsdWrite] = r + fb * yr;
            l = dry * l + wet * yl;
            r = dry * r + wet * yr;

            if (R != nullptr)
            {
                const float pan = Ld * 0.9f * std::sin (twoPi * (float) panPhase);
                const float ang = (pan + 1.0f) * pi * 0.25f;
                l *= std::cos (ang) * juce::MathConstants<float>::sqrt2;
                r *= std::sin (ang) * juce::MathConstants<float>::sqrt2;
            }
        }
        else
        {
            lsdBuf[0][(size_t) lsdWrite] = l;
            lsdBuf[1][(size_t) lsdWrite] = r;
        }
        lsdWrite = (lsdWrite + 1) & bufMask;

        // --- Envoi réverb (immense pour LSD, cotonneuse pour weed)
        const float send = juce::jmin (0.8f, Ld * 0.45f + W * 0.4f + A * 0.1f);
        sL[i] = l * send;
        sR[i] = r * send;

        L[i] = l;
        if (R) R[i] = r;
    }

    if (R) reverb.processStereo (sL, sR, n);
    else   reverb.processMono (sL, n);

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float mix = sMix.next(), og = sOut.next();
        float l = L[i] + sL[i] * 0.7f;
        l = softClip ((dL[i] + mix * (l - dL[i])) * og);
        L[i] = l;
        peak = juce::jmax (peak, std::abs (l));
        if (R)
        {
            float r = R[i] + sR[i] * 0.7f;
            r = softClip ((dR[i] + mix * (r - dR[i])) * og);
            R[i] = r;
            peak = juce::jmax (peak, std::abs (r));
        }
    }
    outputLevel.store (juce::jmax (peak, outputLevel.load() * 0.9f));
}

void PharmacieAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PharmacieAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PharmacieAudioProcessor::createEditor()
{
    return new PharmacieEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PharmacieAudioProcessor();
}
