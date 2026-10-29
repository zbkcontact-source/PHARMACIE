#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dsp2;

namespace
{
    juce::String pct (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }

    inline float peak (SVF& f, float x, float g, float q, float gain)
    {
        const float k = 1.0f / q;
        f.process (x, g, k);
        return x + (gain - 1.0f) * k * f.bp;
    }
    inline float highShelf (SVF& f, float x, float g, float gain)
    {
        f.process (x, g, 1.414f);
        return x + (gain - 1.0f) * f.hp;
    }
    inline float kneeClip (float x)
    {
        const float ax = std::abs (x);
        if (ax < 0.6f) return x;
        return std::copysign (0.6f + 0.35f * std::tanh ((ax - 0.6f) / 0.35f), x);
    }
    inline float toDb (float x) { return x > 1.0e-5f ? 20.0f * std::log10 (x) : -100.0f; }
}

PharmacieAudioProcessor::PharmacieAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto& l : effectLevel) l.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout PharmacieAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pctAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct);
    const juce::NormalisableRange<float> r100 (0.0f, 100.0f, 0.1f);
    auto title = [] (juce::String s) { return s.substring (0, 1) + s.substring (1).toLowerCase(); };

    for (int m = 0; m < Mods::count; ++m)
    {
        const juce::String k (Mods::key (m)), n (Mods::label (m));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { k + "_on", 4 }, n + " On", false));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { k + "_dose", 4 }, n + " Dose", r100, 40.0f, pctAttr));
        for (int w = 0; w < 2; ++w)
        {
            const int mi = m, wi = w;
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { k + (w == 0 ? "_adv" : "_adv2"), 4 }, n + " " + title (Mods::advName (m, w)),
                r100, Mods::advDefault (m, w),
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [mi, wi] (float v, int) { return Mods::advText (mi, wi, v / 100.0f); })));
        }
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { k + "_mix", 4 }, n + " Mix", r100, 100.0f, pctAttr));
    }
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "clean_on", 4 }, "Clean On", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "clean", 4 }, "Clean", r100, 50.0f, pctAttr));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mix", 4 }, "Mix", r100, 100.0f, pctAttr));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "out", 4 }, "Output",
                    juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return juce::String (v, 1) + " dB"; })));
    return layout;
}

float PharmacieAudioProcessor::param (int m, const char* suffix) const
{
    return apvts.getRawParameterValue (juce::String (Mods::key (m)) + suffix)->load();
}

float PharmacieAudioProcessor::doseOf (int m) const
{
    return param (m, "_on") < 0.5f ? 0.0f : param (m, "_dose") / 100.0f;
}

bool PharmacieAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void PharmacieAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const double fs = sr * 2.0; // moteur suréchantillonné x2

    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples (juce::roundToInt (os->getLatencyInSamples()));

    for (int m = 0; m < Mods::count; ++m)
    {
        sDose[(size_t) m].setTime (fs, 0.06); sDose[(size_t) m].snap (0.0f);
        sMix[(size_t) m].setTime (fs, 0.04);  sMix[(size_t) m].snap (param (m, "_mix") / 100.0f);
        sA1[(size_t) m].setTime (fs, 0.08);   sA1[(size_t) m].snap (param (m, "_adv") / 100.0f);
        sA2[(size_t) m].setTime (fs, 0.08);   sA2[(size_t) m].snap (param (m, "_adv2") / 100.0f);
        levelSmooth[m] = 0.0f;
    }
    sGlobalMix.setTime (fs, 0.03); sGlobalMix.snap (apvts.getRawParameterValue ("mix")->load() / 100.0f);
    sOut.setTime (fs, 0.03); sOut.snap (dbToGain (apvts.getRawParameterValue ("out")->load()));
    sClean.setTime (fs, 0.08); sClean.snap (0.0f);

    for (auto* f : { &tapeBump, &tapeLP1, &tapeLP2, &hissHP, &preEm, &deEm, &presence, &xLow, &xHigh, &subBP,
                     &exHP, &airHP, &harshBP, &warmPk, &softHi })
        for (auto& s : *f) s.reset();
    tapeEnv.set (fs, 0.01f, 0.15f);
    envFast.set (fs, 0.0002f, 0.03f); envSus.set (fs, 0.001f, 0.03f); envLong.set (fs, 0.001f, 0.3f);
    for (auto* e : { &tapeEnv, &envFast, &envSlow, &envSus, &envLong }) e->v = 0.0f;
    for (auto& e : bandEnv) { e.set (fs, 0.003f, 0.08f); e.v = 0.0f; }
    glueEnv.set (fs, 0.01f, 0.12f); glueEnv.v = 0.0f;
    harshEnv.set (fs, 0.002f, 0.06f); fullEnv.set (fs, 0.002f, 0.06f); levEnv.set (fs, 0.02f, 0.25f);
    harshEnv.v = fullEnv.v = levEnv.v = 0.0f;

    for (auto& b : grainBuf) b.init ((int) (fs * 4.0));
    for (auto& gr : grains) gr.active = false;
    recIndex = 0; grainAcc = 0.0;

    for (auto& b : chorusBuf) b.init ((int) (fs * 0.05));
    for (auto& ch : phaser) for (auto& a : ch) a.z = 0.0f;
    phFb[0] = phFb[1] = 0.0f; lfoPh = 0.0; freeBeat = 0.0;
    dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
    cleanSmooth = 0.0f;

    juce::Reverb::Parameters cold; cold.roomSize = 0.93f; cold.damping = 0.3f; cold.wetLevel = 1.0f;
    cold.dryLevel = 0.0f; cold.width = 1.0f;
    coldVerb.setSampleRate (fs); coldVerb.setParameters (cold); coldVerb.reset();

    coldSend.setSize (2, samplesPerBlock * 2); dryBuf.setSize (2, samplesPerBlock * 2);
    prepared = true;
}

void PharmacieAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || nch == 0 || ! prepared) return;

    for (int m = 0; m < Mods::count; ++m)
    {
        sDose[(size_t) m].target = doseOf (m);
        sMix[(size_t) m].target = param (m, "_mix") / 100.0f;
        sA1[(size_t) m].target = param (m, "_adv") / 100.0f;
        sA2[(size_t) m].target = param (m, "_adv2") / 100.0f;
    }
    sGlobalMix.target = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    sOut.target = dbToGain (apvts.getRawParameterValue ("out")->load());
    sClean.target = apvts.getRawParameterValue ("clean_on")->load() > 0.5f ? apvts.getRawParameterValue ("clean")->load() / 100.0f : 0.0f;

    double bpm = 120.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (40.0, 240.0, *b);
            if (pos->getIsPlaying()) if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    const double beatStart = ppq >= 0.0 ? ppq : freeBeat;
    const double fs = sr * 2.0;
    const double beatInc = bpm / 60.0 / fs;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int n2 = (int) up.getNumSamples();
    processChain (up.getChannelPointer (0), nch > 1 ? up.getChannelPointer (1) : nullptr, n2, fs, beatStart, beatInc);
    os->processSamplesDown (block);

    freeBeat = beatStart + beatInc * n2;
    beatPhase.store ((float) (freeBeat - std::floor (freeBeat)));
}

void PharmacieAudioProcessor::processChain (float* L, float* R, int n, double fsD, double beat, double beatInc)
{
    const float fs = (float) fsD;
    coldSend.setSize (2, n, false, false, true); dryBuf.setSize (2, n, false, false, true);
    coldSend.clear();
    float* cL = coldSend.getWritePointer (0); float* cR = coldSend.getWritePointer (1);
    float* dL = dryBuf.getWritePointer (0);   float* dR = dryBuf.getWritePointer (1);

    double diffAcc[Mods::count] {}, inAcc = 0.0, cleanAcc = 0.0;

    const float gHiss = gOf (2500.0f, fs), gPres = gOf (4000.0f, fs), gEm = gOf (3000.0f, fs);
    const float gXL = gOf (120.0f, fs), gXH = gOf (3000.0f, fs), gSub = gOf (180.0f, fs), gAir = gOf (10000.0f, fs);
    const float gHarsh = gOf (3800.0f, fs), gWarm = gOf (220.0f, fs), gSoft = gOf (9000.0f, fs);
    const float dcR = 1.0f - 2.0f * pi * 10.0f / fs;

    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R ? R[i] : l;
        dL[i] = l; dR[i] = r;
        inAcc += (double) (l * l + r * r);

        float D[Mods::count], M[Mods::count], A[Mods::count], B[Mods::count];
        for (int m = 0; m < Mods::count; ++m)
        {
            D[m] = sDose[(size_t) m].next(); M[m] = sMix[(size_t) m].next();
            A[m] = sA1[(size_t) m].next(); B[m] = sA2[(size_t) m].next();
        }
        auto apply = [&] (int m, float pl, float pr)
        {
            const float mx = M[m];
            const float nl = l + mx * (pl - l), nr = r + mx * (pr - r);
            diffAcc[m] += (double) ((nl - l) * (nl - l) + (nr - r) * (nr - r));
            l = nl; r = nr;
        };

        // ============ WEED — TAPE (SPEED, HISS)
        if (D[Mods::weed] > 0.0001f)
        {
            const float Dw = D[Mods::weed], sp = A[Mods::weed];
            const float drive = 1.0f + 6.0f * Dw, bias = 0.15f * Dw, tb = std::tanh (bias);
            const float norm = dbToGain (-2.0f * Dw) / std::pow (drive, 0.82f);
            const float bumpF = 55.0f + 70.0f * sp;
            const float bumpG = dbToGain (4.5f * Dw), gBump = gOf (bumpF, fs);
            const float tapeFc = 5500.0f + 14500.0f * std::pow (sp, 1.2f);
            const float gLP = gOf (20000.0f * std::pow (tapeFc / 20000.0f, Dw), fs);
            const float em = dbToGain (6.0f * Dw), dem = 1.0f / em;
            float pl = l, pr = r;
            pl = peak (tapeBump[0], pl, gBump, 0.9f, bumpG); pr = peak (tapeBump[1], pr, gBump, 0.9f, bumpG);
            pl = highShelf (preEm[0], pl, gEm, em);  pr = highShelf (preEm[1], pr, gEm, em);   // pré-emphase
            pl = (std::tanh (pl * drive + bias) - tb) * norm;
            pr = (std::tanh (pr * drive + bias) - tb) * norm;
            pl = highShelf (deEm[0], pl, gEm, dem);  pr = highShelf (deEm[1], pr, gEm, dem);   // dé-emphase
            tapeLP1[0].process (pl, gLP, 1.414f); tapeLP2[0].process (tapeLP1[0].lp, gLP, 1.414f); pl = tapeLP2[0].lp;
            tapeLP1[1].process (pr, gLP, 1.414f); tapeLP2[1].process (tapeLP1[1].lp, gLP, 1.414f); pr = tapeLP2[1].lp;
            const float env = tapeEnv.process (juce::jmax (std::abs (pl), std::abs (pr)));
            const float thr = -6.0f - 10.0f * Dw, ratio = 1.5f + 1.5f * Dw;
            const float over = toDb (env) - thr;
            const float gr = over > 0.0f ? -over * (1.0f - 1.0f / ratio) : 0.0f;
            const float cg = dbToGain (gr * 0.8f + (-thr) * (1.0f - 1.0f / ratio) * 0.25f);
            pl *= cg; pr *= cg;
            const float hiss = std::pow (B[Mods::weed], 1.5f) * 0.03f * juce::jmin (1.0f, Dw * 4.0f);
            if (hiss > 0.0f)
            {
                hissHP[0].process (rng.nextFloat() * 2.0f - 1.0f, gHiss, 1.0f);
                hissHP[1].process (rng.nextFloat() * 2.0f - 1.0f, gHiss, 1.0f);
                pl += hissHP[0].hp * hiss; pr += hissHP[1].hp * hiss;
            }
            apply (Mods::weed, pl, pr);
        }

        // ============ COKE — PUNCH (ATTACK, SUSTAIN)
        if (D[Mods::coke] > 0.0001f)
        {
            const float Dc = D[Mods::coke];
            envSlow.set (fsD, 0.005f + 0.045f * A[Mods::coke], 0.03f);
            const float det = juce::jmax (std::abs (l), std::abs (r));
            const float ef = envFast.process (det), es = envSlow.process (det);
            const float esu = envSus.process (det), el = envLong.process (det);
            const float trans = juce::jlimit (1.0f, 1.0f + 5.0f * Dc, std::pow (ef / (es + 1.0e-5f), 1.4f * Dc));
            const float bip = (B[Mods::coke] - 0.5f) * 2.0f;
            const float ratioS = (esu + 1.0e-5f) / (el + 1.0e-5f);
            const float sus = juce::jlimit (0.25f, 2.5f, std::pow (ratioS, -bip * 1.4f));
            const float susMk = dbToGain (-5.0f * juce::jmax (0.0f, bip));
            float pl = l * trans * sus * susMk, pr = r * trans * sus * susMk;
            const float pg = dbToGain (6.0f * Dc);
            pl = peak (presence[0], pl, gPres, 0.9f, pg); pr = peak (presence[1], pr, gPres, 0.9f, pg);
            const float drive = 1.0f + 2.5f * Dc, mk = dbToGain (-7.0f * std::pow (Dc, 0.8f)) / std::pow (drive, 0.55f);
            pl = kneeClip (pl * drive) * mk; pr = kneeClip (pr * drive) * mk;
            apply (Mods::coke, pl, pr);
        }

        // ============ KETA — GRAIN (SIZE, SPRAY)
        {
            const float Dk = D[Mods::keta];
            if (Dk <= 0.97f) { grainBuf[0].push (l); grainBuf[1].push (r); ++recIndex; }
            if (Dk > 0.0001f)
            {
                const float sizeSec = 0.02f + 0.4f * A[Mods::keta];
                const int len = juce::jmax (64, (int) (sizeSec * fs));
                const float rate = 4.0f + 70.0f * std::pow (Dk, 1.2f);
                grainAcc += rate / fsD;
                while (grainAcc >= 1.0)
                {
                    grainAcc -= 1.0 + (rng.nextFloat() - 0.5f) * 0.6f;
                    for (auto& gr : grains)
                        if (! gr.active)
                        {
                            double back = len + 0.02 * fsD + rng.nextFloat() * B[Mods::keta] * 1.5 * fsD;
                            back = juce::jmin (back, (double) grainBuf[0].mask - len - 16.0, (double) recIndex - 4.0);
                            if (back < (double) len) break;
                            gr.active = true; gr.len = len; gr.age = 0;
                            gr.start = recIndex - (long long) back;
                            gr.rev = rng.nextFloat() < 0.25f * Dk;
                            const float pan = (rng.nextFloat() * 2.0f - 1.0f) * Dk;
                            const float ang = (pan + 1.0f) * pi * 0.25f;
                            gr.gl = std::cos (ang) * 1.41421356f; gr.gr = std::sin (ang) * 1.41421356f;
                            break;
                        }
                }
                float gl = 0.0f, grr = 0.0f;
                for (auto& gr : grains)
                {
                    if (! gr.active) continue;
                    const float u = (float) gr.age / (float) gr.len;
                    const float w = 0.5f - 0.5f * std::cos (twoPi * u);
                    const long long idx = gr.start + (gr.rev ? gr.len - 1 - gr.age : gr.age);
                    gl += grainBuf[0].at ((int) (idx & 0x7fffffff)) * w * gr.gl;
                    grr += grainBuf[1].at ((int) (idx & 0x7fffffff)) * w * gr.gr;
                    if (++gr.age >= gr.len) gr.active = false;
                }
                const float gn = (1.15f - 0.45f * Dk) / std::sqrt (juce::jmax (1.0f, rate * sizeSec * 0.5f));
                gl *= gn; grr *= gn;
                const float wet = juce::jmin (1.0f, Dk * 1.6f);
                cL[i] += gl * 0.35f * Dk * M[Mods::keta]; cR[i] += grr * 0.35f * Dk * M[Mods::keta];
                apply (Mods::keta, l + wet * (gl - l), r + wet * (grr - r));
            }
        }

        // ============ LSD — COLOR (RATE, DEPTH)
        {
            const float Dl = D[Mods::lsd];
            chorusBuf[0].push (l); chorusBuf[1].push (r);
            if (Dl > 0.0001f)
            {
                const float rate = 0.1f * std::pow (60.0f, A[Mods::lsd]), dep = B[Mods::lsd];
                lfoPh += rate / fsD; wrap (lfoPh);
                auto tri = [] (double p) { p -= std::floor (p); return (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); };
                const float depth = (0.0005f + 0.0045f * dep) * fs;
                const float b1 = 0.007f * fs, b2 = 0.011f * fs;
                const float v1l = chorusBuf[0].read (b1 + depth * tri (lfoPh));
                const float v1r = chorusBuf[1].read (b1 + depth * tri (lfoPh + 0.5));
                const float v2l = chorusBuf[0].read (b2 + depth * 0.7f * tri (lfoPh + 0.25));
                const float v2r = chorusBuf[1].read (b2 + depth * 0.7f * tri (lfoPh + 0.75));
                const float cw = 0.5f * juce::jmin (1.0f, Dl * 2.0f);
                float o[2] = { l * (1.0f - 0.3f * cw) + cw * (0.6f * v1l + 0.4f * v2r),
                               r * (1.0f - 0.3f * cw) + cw * (0.6f * v1r + 0.4f * v2l) };
                for (int c = 0; c < 2; ++c)
                {
                    const float s = std::sin (twoPi * (float) lfoPh * 0.5f + (c == 0 ? 0.0f : pi * 0.5f));
                    const float f = 300.0f * std::pow (12.0f, 0.5f + 0.5f * s);
                    const float tg = std::tan (pi * juce::jmin (f, 0.45f * fs) / fs);
                    const float a = (tg - 1.0f) / (tg + 1.0f);
                    float y = o[c] + phFb[c] * (0.2f + 0.5f * dep) * Dl;
                    for (auto& st : phaser[(size_t) c]) y = st.process (y, a);
                    phFb[c] = y;
                    o[c] += (y - o[c]) * 0.5f * Dl;
                }
                const float mid = 0.5f * (o[0] + o[1]), side = 0.5f * (o[0] - o[1]) * (1.0f + 0.7f * Dl);
                const float pl = mid + side, pr = mid - side;
                cL[i] += pl * 0.12f * Dl * M[Mods::lsd]; cR[i] += pr * 0.12f * Dl * M[Mods::lsd];
                apply (Mods::lsd, pl, pr);
            }
        }

        // ============ MDMA — ENRICH (TONE, PULSE)
        if (D[Mods::mdma] > 0.0001f)
        {
            const float Dm = D[Mods::mdma], T = A[Mods::mdma], P = B[Mods::mdma];
            float out[2] = { l, r };
            for (int c = 0; c < 2; ++c)
            {
                const float x = c == 0 ? l : r;
                xLow[(size_t) c].process (x, gXL, 1.414f);
                xHigh[(size_t) c].process (x, gXH, 1.414f);
                const float low = xLow[(size_t) c].lp, high = xHigh[(size_t) c].hp, mid = x - low - high;
                subBP[(size_t) c].process (std::tanh (low * 4.0f) * 0.5f, gSub, 1.4f);
                exHP[(size_t) c].process (std::tanh (high * 4.0f) * 0.5f, gXH, 1.414f);
                airHP[(size_t) c].process (x, gAir, 1.414f);
                float y = x + subBP[(size_t) c].bp * Dm * (1.0f - T) * 1.2f
                            + low * (dbToGain (3.0f * Dm * (1.0f - T)) - 1.0f)
                            + exHP[(size_t) c].hp * Dm * T * 0.9f
                            + airHP[(size_t) c].hp * (dbToGain (5.0f * Dm * T) - 1.0f);
                const float bands[3] = { low, mid, high };
                float ott = 0.0f;
                for (int b = 0; b < 3; ++b)
                {
                    const float e = c == 0 ? bandEnv[(size_t) b].process (std::abs (bands[b])) : bandEnv[(size_t) b].v;
                    ott += bands[b] * juce::jlimit (0.3f, 3.5f, std::sqrt (0.12f / (e + 1.0e-4f)));
                }
                out[c] = y + 0.35f * Dm * (ott - x);
            }
            const float env = glueEnv.process (juce::jmax (std::abs (out[0]), std::abs (out[1])));
            const float thr = -4.0f - 8.0f * Dm, over = toDb (env) - thr;
            const float gg = dbToGain ((over > 0.0f ? -over * 0.5f : 0.0f) + (-thr) * 0.25f * Dm - 4.0f * Dm);
            float pl = out[0] * gg, pr = out[1] * gg;
            const float mid = 0.5f * (pl + pr), side = 0.5f * (pl - pr) * (1.0f + 0.4f * Dm);
            pl = softClip (mid + side); pr = softClip (mid - side);
            // PULSE : trémolo calé sur les croches du tempo, léger décalage stéréo
            if (P > 0.001f)
            {
                const double e8 = beat * 2.0;
                const float ph = (float) (e8 - std::floor (e8));
                const float pr2 = ph + 0.08f * P;
                const float gL = 1.0f - 0.9f * P * 0.5f * (1.0f - std::cos (twoPi * ph));
                const float gR = 1.0f - 0.9f * P * 0.5f * (1.0f - std::cos (twoPi * (pr2 - std::floor (pr2))));
                pl *= gL; pr *= gR;
            }
            apply (Mods::mdma, pl, pr);
        }

        beat += beatInc;
        L[i] = l;
        if (R) R[i] = r;
    }

    if (R) coldVerb.processStereo (cL, cR, n);
    else   coldVerb.processMono (cL, n);

    float peakLvl = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float gm = sGlobalMix.next(), og = sOut.next(), C = sClean.next();
        float o[2] = { L[i] + cL[i] * 0.7f, (R ? R[i] : L[i]) + cR[i] * 0.7f };

        // ============ CLEAN : de-harsh dynamique, chaleur, aigus adoucis, leveler doux
        if (C > 0.0001f)
        {
            const float be = harshEnv.process (juce::jmax (std::abs (harshBP[0].bp), std::abs (harshBP[1].bp)));
            const float fe = fullEnv.process (juce::jmax (std::abs (o[0]), std::abs (o[1])));
            const float red = juce::jlimit (0.0f, 8.0f, (toDb (be) - toDb (fe) + 9.0f) * 0.8f) * C;
            const float cut = 1.0f - dbToGain (-red);
            const float warm = dbToGain (1.8f * C), soft = dbToGain (-2.5f * C);
            const float lv = levEnv.process (juce::jmax (std::abs (o[0]), std::abs (o[1])));
            const float over = toDb (lv) + 18.0f;
            const float ratio = 1.0f + 1.5f * C;
            const float lg = dbToGain ((over > 0.0f ? -over * (1.0f - 1.0f / ratio) : 0.0f) + 3.0f * C * (1.0f - 1.0f / ratio));
            for (int c = 0; c < 2; ++c)
            {
                float x = o[c];
                harshBP[(size_t) c].process (x, gHarsh, 1.0f / 0.8f);
                x -= cut * (1.0f / 0.8f) * harshBP[(size_t) c].bp;
                x = peak (warmPk[(size_t) c], x, gWarm, 0.7f, warm);
                x = highShelf (softHi[(size_t) c], x, gSoft, soft);
                o[c] = x * lg;
            }
            cleanAcc += red;
        }

        for (int c = 0; c < (R ? 2 : 1); ++c)
        {
            const float dry = c == 0 ? dL[i] : dR[i];
            // anti-DC appliqué seulement à ce que le plugin ajoute : sans effet actif, le son reste intact
            const float diff = o[c] - dry;
            const float dd = diff - dcX[c] + dcR * dcY[c];
            dcX[c] = diff; dcY[c] = dd;
            float y = softClip ((dry + gm * dd) * og);
            (c == 0 ? L : R)[i] = y;
            peakLvl = juce::jmax (peakLvl, std::abs (y));
        }
    }
    outputLevel.store (juce::jmax (peakLvl, outputLevel.load() * 0.9f));
    cleanSmooth += ((float) (cleanAcc / n) / 8.0f - cleanSmooth) * 0.3f;
    cleanLevel.store (cleanSmooth);

    const double inRms = std::sqrt (inAcc / (2.0 * n));
    for (int m = 0; m < Mods::count; ++m)
    {
        const double d = std::sqrt (diffAcc[m] / (2.0 * n));
        float target = inRms > 1.0e-4 ? (float) juce::jmin (1.0, d / (inRms * 0.9)) : 0.0f;
        if (sDose[(size_t) m].target <= 0.0f) target = 0.0f;
        levelSmooth[m] += (target - levelSmooth[m]) * (target > levelSmooth[m] ? 0.5f : 0.15f);
        effectLevel[(size_t) m].store (levelSmooth[m]);
    }
}

void PharmacieAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void PharmacieAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PharmacieAudioProcessor::createEditor() { return new PharmacieEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PharmacieAudioProcessor(); }
