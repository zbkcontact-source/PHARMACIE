#pragma once

#include <cmath>
#include <vector>
#include <array>
#include <juce_core/juce_core.h>

namespace dsp2
{
    constexpr float pi = 3.14159265358979f;
    constexpr float twoPi = 6.28318530717959f;

    inline void wrap (double& p) { p -= std::floor (p); }

    // Filtre à variable d'état (TPT) : sorties passe-bas, passe-bande, passe-haut
    struct SVF
    {
        float ic1 = 0.0f, ic2 = 0.0f, lp = 0.0f, bp = 0.0f, hp = 0.0f;
        void reset() { ic1 = ic2 = lp = bp = hp = 0.0f; }
        void process (float v0, float g, float k)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1, a3 = g * a2;
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
    };

    inline float gOf (float fc, float sr) { return std::tan (pi * juce::jlimit (10.0f, 0.45f * sr, fc) / sr); }

    struct Smooth
    {
        float value = 0.0f, target = 0.0f, coeff = 0.001f;
        void setTime (double sr, double s) { coeff = (float) (1.0 - std::exp (-1.0 / (s * sr))); }
        void snap (float v) { value = target = v; }
        float next() { value += coeff * (target - value); return value; }
    };

    // Ligne de retard circulaire avec lecture interpolée
    struct Delay
    {
        std::vector<float> b;
        int mask = 0, w = 0;
        void init (int minSize)
        {
            int n = 1; while (n < minSize) n <<= 1;
            b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
        }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
        void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
        // d = retard en échantillons (>= 1)
        float read (float d) const
        {
            const double rp = (double) w - (double) d + (double) (mask + 1);
            const int i0 = (int) rp;
            const float fr = (float) (rp - (double) i0);
            const float a = b[(size_t) (i0 & mask)], c = b[(size_t) ((i0 + 1) & mask)];
            return a + fr * (c - a);
        }
        float at (int absIndex) const { return b[(size_t) (absIndex & mask)]; }
    };

    // Changement de hauteur à deux têtes de lecture croisées (style "bande")
    struct PitchShifter
    {
        Delay d;
        double phase = 0.0;
        float win = 3000.0f;
        void init (double sr, float windowSec)
        {
            win = (float) (windowSec * sr);
            d.init ((int) win * 2 + 8);
            phase = 0.0;
        }
        void reset() { d.clear(); phase = 0.0; }
        float process (float x, float ratio)
        {
            d.push (x);
            phase += (1.0 - (double) ratio) / (double) win;
            wrap (phase);
            double p2 = phase + 0.5; wrap (p2);
            const float s1 = std::sin (pi * (float) phase), s2 = std::sin (pi * (float) p2);
            return s1 * s1 * d.read ((float) phase * win + 2.0f) + s2 * s2 * d.read ((float) p2 * win + 2.0f);
        }
    };

    // Version stéréo qui partage la phase (garde l'image stéréo)
    struct StereoPitch
    {
        std::array<Delay, 2> d;
        double phase = 0.0;
        float win = 3000.0f;
        void init (double sr, float windowSec)
        {
            win = (float) (windowSec * sr);
            for (auto& x : d) x.init ((int) win * 2 + 8);
            phase = 0.0;
        }
        void reset() { for (auto& x : d) x.clear(); phase = 0.0; }
        void process (float& l, float& r, float ratio)
        {
            d[0].push (l); d[1].push (r);
            phase += (1.0 - (double) ratio) / (double) win;
            wrap (phase);
            double p2 = phase + 0.5; wrap (p2);
            const float s1 = std::sin (pi * (float) phase), s2 = std::sin (pi * (float) p2);
            const float g1 = s1 * s1, g2 = s2 * s2;
            const float d1 = (float) phase * win + 2.0f, d2 = (float) p2 * win + 2.0f;
            l = g1 * d[0].read (d1) + g2 * d[0].read (d2);
            r = g1 * d[1].read (d1) + g2 * d[1].read (d2);
        }
    };

    struct Allpass1
    {
        float z = 0.0f;
        float process (float x, float a) { const float y = -a * x + z; z = x + a * y; return y; }
    };

    inline float softClip (float x)
    {
        const float ax = std::abs (x);
        if (ax < 1.0f) return x;
        return std::copysign (1.0f + 0.5f * std::tanh ((ax - 1.0f) / 0.5f), x);
    }

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
}
