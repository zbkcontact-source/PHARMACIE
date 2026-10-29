#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    String U (const char* s) { return String::fromUTF8 (s); }

    float clamp01 (float x) { return jlimit (0.0f, 1.0f, x); }
    float lerp (float a, float b, float t) { return a + (b - a) * t; }
    Colour mixC (Colour a, Colour b, float t) { return a.interpolatedWith (b, clamp01 (t)); }

    Point<float> quad (Point<float> a, Point<float> c, Point<float> b, float t)
    {
        const float u = 1.0f - t;
        return a * (u * u) + c * (2.0f * u * t) + b * (t * t);
    }

    Path heartPath (float cx, float cy, float s)
    {
        Path p;
        p.startNewSubPath (cx, cy + s * 0.9f);
        p.cubicTo (cx - s * 1.6f, cy - s * 0.1f, cx - s * 0.8f, cy - s * 1.2f, cx, cy - s * 0.45f);
        p.cubicTo (cx + s * 0.8f, cy - s * 1.2f, cx + s * 1.6f, cy - s * 0.1f, cx, cy + s * 0.9f);
        p.closeSubPath();
        return p;
    }

    Path crossPath (float cx, float cy, float s)
    {
        Path p;
        const float a = s * 0.18f, b = s * 0.5f;
        p.addRoundedRectangle (cx - a, cy - b, 2 * a, 2 * b, a * 0.4f);
        p.addRoundedRectangle (cx - b, cy - a, 2 * b, 2 * a, a * 0.4f);
        return p;
    }

    const char* feelings[Mods::count][3] = {
        { "chilled", "everything’s slow", "melting into the couch" },
        { "a bit wired", "talking too fast", "heart at 300 bpm" },
        { "slightly off", "floating", "K-hole" },
        { "colors are humming", "the walls are breathing", "dissolved into the universe" },
        { "coming up", "I love everyone", "jaw’s gone" } };

    const Colour ink { 0xff14080f };
    const Colour lidCol { 0xff260d1c };
    const Colour mouthCol { 0xff76465f };
    const Colour skinCol { 0xff886379 };
}

//==============================================================================
PharmacieLookAndFeel::PharmacieLookAndFeel()
{
    setColour (Label::textColourId, Palette::fg);
}

void PharmacieLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                             float a0, float a1, Slider& s)
{
    const auto col = s.findColour (Slider::rotarySliderFillColourId);
    const auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float arcR = radius - 4.0f, ang = a0 + pos * (a1 - a0);

    Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (Palette::line);
    g.strokePath (track, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    if (pos > 0.001f)
    {
        Path fill; fill.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (fill, PathStrokeType (12.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (fill, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    const float kr = radius * 0.66f;
    ColourGradient grad (Colour (0xff3a434e), c.x, c.y - kr, Colour (0xff1a1f25), c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (Rectangle<float> (kr * 2, kr * 2).withCentre (c));
    g.setColour (Colour (0xff4a5561));
    g.drawEllipse (Rectangle<float> (kr * 2, kr * 2).withCentre (c), 1.0f);
    g.setColour (Palette::fg);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.3f, ang), c.getPointOnCircumference (kr * 0.85f, ang) }, 3.0f);
}

void PharmacieLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                             Slider::SliderStyle style, Slider& s)
{
    const auto col = s.findColour (Slider::trackColourId);
    if (style == Slider::LinearVertical)
    {
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (Palette::line);
        g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 3.0f);
        g.setColour (col);
        g.fillRoundedRectangle (cx - 3.0f, pos, 6.0f, (float) (y + h) - pos, 3.0f);
        auto cap = Rectangle<float> ((float) w * 0.8f, 12.0f).withCentre ({ cx, pos });
        g.setColour (Colour (0xff3a434e)); g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (Palette::fg); g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 8.0f, 2.0f));
    }
    else
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (Palette::line);
        g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
        g.setColour (col);
        g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
        g.setColour (Palette::fg);
        g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    }
}

void PharmacieLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool hi, bool)
{
    const bool on = b.getToggleState();
    const auto col = b.findColour (ToggleButton::tickColourId);
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (on ? col.withAlpha (0.18f) : Palette::panel2);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? col : (hi ? Palette::muted : Palette::line));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
    const auto led = Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() });
    if (on) { g.setColour (col.withAlpha (0.4f)); g.fillEllipse (led.expanded (3.0f)); }
    g.setColour (on ? col : Palette::line);
    g.fillEllipse (led);
    g.setColour (on ? Palette::fg : Palette::muted);
    g.setFont (FontOptions (12.0f, Font::bold));
    const String txt = b.getButtonText().isNotEmpty() ? b.getButtonText() : String (on ? "ON" : "OFF");
    g.drawText (txt, r.withTrimmedLeft (22.0f), Justification::centredLeft);
}

Label* PharmacieLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (Label::textColourId, Palette::fg);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setFont (FontOptions (12.0f));
    return l;
}

//==============================================================================
Visualizer::Visualizer()
{
    shop = ImageCache::getFromMemory (BinaryData::shop_jpg, BinaryData::shop_jpgSize);
    parts.reserve (500);
    setOpaque (true);
}

void Visualizer::resized()
{
    k = (float) getWidth() / 3072.0f;
    const float w = (float) getWidth(), h = (float) getHeight();

    GlyphArrangement ga;
    ga.addLineOfText (Font (FontOptions (h * 0.15f, Font::bold)), "PHARMACY", 0.0f, 0.0f);
    neonText.clear();
    ga.createPath (neonText);
    auto bb = neonText.getBounds();
    const float baseline = h * 0.138f;
    neonText.applyTransform (AffineTransform::translation (w * 0.5f - bb.getCentreX(), baseline));
    bb = neonText.getBounds();
    const float cs = bb.getHeight() * 0.95f;
    neonCrossL = crossPath (bb.getX() - cs * 0.95f, bb.getCentreY(), cs);
    neonCrossR = crossPath (bb.getRight() + cs * 0.95f, bb.getCentreY(), cs);
}

void Visualizer::spawn (int type, float x, float y, float vx, float vy, float life, float size, float hue)
{
    if (parts.size() > 450) return;
    parts.push_back ({ x, y, vx, vy, life, life, size, rnd.nextFloat() * 6.28f, (rnd.nextFloat() - 0.5f) * 2.0f, hue, type });
}

void Visualizer::tick (const float doses[Mods::count], const float levels[Mods::count], float outLevel, float beat, float clean)
{
    const float dt = 1.0f / 30.0f;
    t += dt;
    for (int i = 0; i < Mods::count; ++i) { d[i] = doses[i]; lv[i] = levels[i]; }
    level = outLevel;
    cl = clean;
    beatPh = beat;
    const float W = d[Mods::weed], C = d[Mods::coke], K = d[Mods::keta], L = d[Mods::lsd], M = d[Mods::mdma];
    const float w = (float) getWidth(), h = (float) getHeight();
    const float hx = w * 0.502f, hy = h * 0.47f; // centre de la tête

    // regard : la coke fait des saccades, la weed traîne, la keta fixe le vide
    lookTimer -= dt;
    if (lookTimer <= 0.0f)
    {
        lookTarget = (rnd.nextFloat() - 0.5f) * (0.3f + C * 1.4f) * (1.0f - K * 0.9f);
        lookTimer = lerp (2.5f, 0.18f, C) + rnd.nextFloat() * (1.0f - C);
    }
    lookX += (lookTarget - lookX) * (C > 0.2f ? 0.6f : lerp (0.08f, 0.02f, W));

    // tremblement
    shakeX = (rnd.nextFloat() - 0.5f) * C * 7.0f * (1.0f - 0.8f * cl);
    shakeY = (rnd.nextFloat() - 0.5f) * C * 5.0f * (1.0f - 0.8f * cl);

    // néon qui grésille
    neonAlpha = 1.0f - K * 0.35f * (0.5f + 0.5f * std::sin (t * 0.8f));
    if (rnd.nextFloat() < 0.012f + C * 0.25f) neonAlpha *= 0.25f + rnd.nextFloat() * 0.5f;

    // particules
    const float act = 0.6f + jmin (1.0f, level * 1.5f);
    auto emit = [&] (int slot, float rate, auto fn)
    {
        spawnAcc[slot] += rate * dt;
        while (spawnAcc[slot] >= 1.0f) { spawnAcc[slot] -= 1.0f; fn(); }
    };
    emit (0, W * 7.0f * act, [&] {
        spawn (0, w * (0.2f + rnd.nextFloat() * 0.6f), h * (0.95f + rnd.nextFloat() * 0.1f),
               (rnd.nextFloat() - 0.5f) * 16.0f, -(22.0f + rnd.nextFloat() * 25.0f), 4.0f + rnd.nextFloat() * 2.5f,
               h * (0.05f + rnd.nextFloat() * 0.06f)); });
    emit (1, (W > 0.45f ? (W - 0.45f) * 4.0f : 0.0f), [&] {
        spawn (0, w * 0.503f + (rnd.nextFloat() - 0.5f) * 30.0f, h * 0.56f, (rnd.nextFloat() - 0.5f) * 20.0f,
               -(15.0f + rnd.nextFloat() * 15.0f), 3.5f, h * 0.035f); });
    emit (2, C * 30.0f * act, [&] {
        const float a = rnd.nextFloat() * 6.283f, sp = 180.0f + rnd.nextFloat() * 220.0f;
        spawn (1, hx + std::cos (a) * h * 0.42f, hy + std::sin (a) * h * 0.4f, std::cos (a) * sp, std::sin (a) * sp,
               0.25f + rnd.nextFloat() * 0.35f, 2.0f); });
    emit (3, K * 3.0f, [&] {
        spawn (2, rnd.nextFloat() * w, rnd.nextFloat() * h, (rnd.nextFloat() - 0.5f) * 10.0f, (rnd.nextFloat() - 0.5f) * 6.0f,
               6.0f + rnd.nextFloat() * 3.0f, h * (0.18f + rnd.nextFloat() * 0.18f)); });
    emit (4, L * 16.0f * act, [&] {
        spawn (3, rnd.nextFloat() * w, rnd.nextFloat() * h, (rnd.nextFloat() - 0.5f) * 20.0f, (rnd.nextFloat() - 0.5f) * 20.0f,
               1.5f + rnd.nextFloat() * 2.5f, 3.0f + rnd.nextFloat() * 6.0f, rnd.nextFloat()); });
    emit (5, M * 5.0f * act, [&] {
        spawn (4, w * (0.08f + rnd.nextFloat() * 0.84f), h * 1.02f, (rnd.nextFloat() - 0.5f) * 15.0f,
               -(40.0f + rnd.nextFloat() * 40.0f), 4.0f + rnd.nextFloat() * 2.0f, 7.0f + rnd.nextFloat() * 9.0f); });
    emit (7, cl * 7.0f, [&] {
        spawn (6, w * (0.05f + rnd.nextFloat() * 0.9f), h * (0.6f + rnd.nextFloat() * 0.45f), (rnd.nextFloat() - 0.5f) * 6.0f,
               -(8.0f + rnd.nextFloat() * 12.0f), 5.0f + rnd.nextFloat() * 3.0f, 6.0f + rnd.nextFloat() * 14.0f); });
    emit (6, M * 9.0f + L * 4.0f, [&] {
        spawn (5, rnd.nextFloat() * w, rnd.nextFloat() * h, 0.0f, -5.0f, 0.6f + rnd.nextFloat() * 0.6f,
               3.0f + rnd.nextFloat() * 4.0f); });

    for (auto& p : parts)
    {
        p.life -= dt;
        p.x += p.vx * dt; p.y += p.vy * dt; p.rot += p.vr * dt;
        if (p.type == 0) { p.size *= 1.0f + 0.22f * dt; p.vx += (rnd.nextFloat() - 0.5f) * 10.0f * dt; }
        if (p.type == 4) p.vx = std::sin (t * 2.0f + p.rot) * 12.0f;
    }
    parts.erase (std::remove_if (parts.begin(), parts.end(), [] (const Particle& p) { return p.life <= 0.0f; }), parts.end());

    repaint();
}

void Visualizer::paint (Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float W = d[Mods::weed], C = d[Mods::coke], K = d[Mods::keta], L = d[Mods::lsd], M = d[Mods::mdma];
    g.fillAll (Colours::black);

    // ---- mouvement de toute la scène
    const float hx = w * 0.502f, hy = h * 0.47f;
    const float bump = M * 0.03f * std::pow (1.0f - beatPh, 3.0f);
    const float scale = 1.045f + 0.02f * K + bump;
    const float rot = W * 0.022f * std::sin (t * 0.6f) + L * 0.018f * std::sin (t * 1.3f) + K * 0.02f * std::sin (t * 0.35f);
    const float ox = shakeX + K * 9.0f * std::sin (t * 0.45f) + W * 4.0f * std::sin (t * 0.5f);
    const float oy = shakeY + K * 6.0f * std::cos (t * 0.37f) + W * 3.0f;
    const auto scene = AffineTransform::translation (-hx, -hy).rotated (rot).scaled (scale).translated (hx + ox, hy + oy);

    {
        Graphics::ScopedSaveState s (g);
        g.addTransform (scene);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        g.drawImage (shop, Rectangle<float> (0, 0, w, h));

        // dédoublement façon keta
        if (K > 0.05f)
        {
            g.setOpacity (0.22f * K);
            g.drawImage (shop, Rectangle<float> (0, 0, w, h).translated (K * 14.0f * std::sin (t * 0.9f), K * 6.0f));
            g.setOpacity (1.0f);
        }

        // bocaux qui s'allument dans la couleur du module
        static const float jars[10][2] = { { 150, 350 }, { 330, 560 }, { 180, 800 }, { 320, 1050 }, { 200, 1300 },
                                           { 2750, 330 }, { 2900, 600 }, { 2700, 880 }, { 2920, 1130 }, { 2780, 1380 } };
        for (int j = 0; j < 10; ++j)
        {
            const int m = j % 5;
            const float a = clamp01 (0.18f * d[m] + 0.55f * lv[m]) * (0.75f + 0.25f * std::sin (t * 3.0f + (float) j));
            if (a < 0.01f) continue;
            const auto c = P (jars[j][0], jars[j][1]);
            const float r = 120.0f * k * (1.0f + lv[m] * 0.6f);
            ColourGradient gr (Palette::mod (m).withAlpha (a * 0.8f), c.x, c.y, Palette::mod (m).withAlpha (0.0f), c.x + r, c.y, true);
            g.setGradientFill (gr);
            g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
        }

        drawNeon (g);
        drawFace (g);
    }

    drawOverlays (g);

    // cadre de l'écran
    g.setColour (Palette::neon.withAlpha (0.35f));
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}

void Visualizer::drawNeon (Graphics& g)
{
    const float L = d[Mods::lsd];
    Colour tube = Palette::neon.interpolatedWith (Colour (0xffffd59a), 0.45f * cl);
    if (L > 0.01f) tube = tube.interpolatedWith (Colour::fromHSV (std::fmod (t * 0.15f, 1.0f), 0.8f, 1.0f, 1.0f), L * 0.75f);
    const float a = neonAlpha;
    Path all = neonText; all.addPath (neonCrossL); all.addPath (neonCrossR);

    g.setColour (tube.withAlpha (0.05f * a)); g.strokePath (all, PathStrokeType (22.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (tube.withAlpha (0.10f * a)); g.strokePath (all, PathStrokeType (12.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (tube.withAlpha (0.30f * a)); g.strokePath (all, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (tube.withAlpha (0.95f * a)); g.strokePath (all, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.interpolatedWith (tube, 0.3f).withAlpha (0.9f * a));
    g.strokePath (all, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
}

void Visualizer::drawEye (Graphics& g, float cx, float cy, float rx, float ry, float lidFrac, float look, int side)
{
    const float W = d[Mods::weed], C = d[Mods::coke], K = d[Mods::keta], L = d[Mods::lsd], M = d[Mods::mdma];
    const float lw = 7.0f * k;

    // cernes
    const float bagA = clamp01 (0.15f * W + 0.8f * C + 0.9f * K + 0.45f * M + 0.2f * L) * (1.0f - 0.6f * cl);
    if (bagA > 0.01f)
    {
        Path bag;
        bag.startNewSubPath (cx - rx * 0.95f, cy + ry * 0.2f);
        bag.quadraticTo (cx, cy + ry * (1.45f + 0.35f * bagA), cx + rx * 0.95f, cy + ry * 0.2f);
        bag.quadraticTo (cx, cy + ry * 0.9f, cx - rx * 0.95f, cy + ry * 0.2f);
        g.setColour (Colour (0xff3e1631).withAlpha (0.7f * bagA));
        g.fillPath (bag);
    }

    Path eye; eye.addEllipse (cx - rx, cy - ry, 2 * rx, 2 * ry);
    const float top = cy - ry, bottom = cy + ry;
    const float lidY = top + clamp01 (lidFrac) * 2.0f * ry;

    Colour sclera (0xffb1d1d5);
    sclera = mixC (sclera, Colour (0xffe9aab0), 0.75f * W);
    sclera = mixC (sclera, Colour (0xffeef6fa), 0.5f * C);
    sclera = mixC (sclera, Colour (0xffc9d0d6), 0.5f * K);

    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (eye);
        g.setColour (sclera);
        g.fillPath (eye);

        if (W > 0.2f) // yeux rouges
        {
            g.setColour (Colour (0xffd2333f).withAlpha ((W - 0.2f) * 0.9f));
            for (int v = 0; v < 4; ++v)
            {
                const float sy = cy - ry * 0.2f + (float) v * ry * 0.22f;
                const float sx = side == 0 ? cx - rx : cx + rx, dir = side == 0 ? 1.0f : -1.0f;
                Path vein; vein.startNewSubPath (sx, sy);
                vein.lineTo (sx + dir * rx * 0.25f, sy + ry * 0.06f);
                vein.lineTo (sx + dir * rx * 0.45f, sy - ry * 0.04f);
                g.strokePath (vein, PathStrokeType (2.2f * k * 3.0f * 0.5f));
            }
        }

        // pupille
        float pr = 15.0f * k * (1.0f + 2.2f * L + 1.6f * M) * (1.0f - 0.6f * C) * (1.0f - 0.55f * K);
        pr = jmax (3.5f * k, pr);
        float pf = 0.1f + 0.38f * clamp01 (C + K + L + 0.9f * M);
        float py = lidY + (bottom - lidY) * pf;
        float px = cx - 4.0f * k + look * rx * 0.45f + (side == 0 ? -1.0f : 1.0f) * K * rx * 0.22f;
        if (L > 0.01f) { px += std::cos (t * 1.7f) * L * rx * 0.12f; py += std::sin (t * 1.7f) * L * ry * 0.08f; }
        py = jmax (py, lidY - pr * 0.5f);

        const float sA = clamp01 ((L - 0.35f) / 0.4f), hA = clamp01 ((M - 0.45f) / 0.35f);
        const float normA = 1.0f - jmax (sA, hA);
        if (normA > 0.01f)
        {
            g.setColour (Colour (0xff050305).withAlpha (normA));
            g.fillEllipse (Rectangle<float> (2 * pr, 2 * pr).withCentre ({ px, py }));
            if (pr > 10.0f * k || cl > 0.05f)
            {
                const float hr = jmax (pr * 0.5f, 7.0f * k * cl);
                g.setColour (Colours::white.withAlpha (jmax (pr > 10.0f * k ? 0.8f : 0.0f, 0.9f * cl) * normA));
                g.fillEllipse (Rectangle<float> (hr, hr).withCentre ({ px - pr * 0.35f, py - pr * 0.35f }));
            }
        }
        if (sA > 0.01f) // spirale arc-en-ciel
        {
            const float R = jmax (pr * 1.2f, 32.0f * k);
            const int segs = 90;
            Point<float> prev { px, py };
            for (int i = 1; i <= segs; ++i)
            {
                const float u = (float) i / (float) segs;
                const float a = u * 6.283f * 3.0f + t * 4.0f * (side == 0 ? 1.0f : -1.0f);
                const Point<float> pt { px + std::cos (a) * R * u, py + std::sin (a) * R * u };
                g.setColour (Colour::fromHSV (std::fmod (u + t * 0.3f, 1.0f), 0.85f, 1.0f, sA));
                g.drawLine ({ prev, pt }, 4.0f * k * 1.6f);
                prev = pt;
            }
        }
        if (hA > 0.01f) // cœurs
        {
            const float pulse = 1.0f + 0.25f * std::pow (1.0f - beatPh, 2.0f);
            auto hp = heartPath (px, py, jmax (pr, 26.0f * k) * 1.05f * pulse);
            g.setColour (Colour (0xffff4fa3).withAlpha (hA)); g.fillPath (hp);
            g.setColour (ink.withAlpha (hA)); g.strokePath (hp, PathStrokeType (3.0f * k));
        }

        if (K > 0.05f) // regard vitreux
        {
            Path gl; gl.addCentredArc (cx + rx * 0.2f, cy - ry * 0.05f, rx * 0.55f, ry * 0.55f, 0.0f, 0.3f, 1.2f, true);
            g.setColour (Colours::white.withAlpha (0.55f * K));
            g.strokePath (gl, PathStrokeType (5.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // paupière
        g.setColour (lidCol);
        g.fillRect (Rectangle<float> (cx - rx - 2, top - 2, 2 * rx + 4, lidY - top + 2));
    }

    // ligne de paupière
    const float yy = clamp01 ((lidY - cy) / ry);
    const float dx = rx * std::sqrt (jmax (0.0f, 1.0f - yy * yy));
    if (lidY > top + 2.0f * k && lidY < bottom - 2.0f * k)
    {
        Path lid; lid.startNewSubPath (cx - dx, lidY);
        lid.quadraticTo (cx, lidY + 7.0f * k, cx + dx, lidY);
        g.setColour (ink);
        g.strokePath (lid, PathStrokeType (lw * 0.9f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setColour (ink);
    g.strokePath (eye, PathStrokeType (lw));

    // plis sous l'oeil
    const int arcs = 2 + (bagA > 0.45f ? 1 : 0);
    for (int a = 0; a < arcs; ++a)
    {
        Path p;
        const float off = ry * (1.05f + 0.13f * (float) a) + bagA * ry * 0.12f * (float) a;
        const float span = rx * (0.72f - 0.12f * (float) a);
        p.startNewSubPath (cx - span, cy + off - ry * 0.18f);
        p.quadraticTo (cx, cy + off + ry * 0.12f, cx + span, cy + off - ry * 0.18f);
        g.setColour (ink.withAlpha (a < 2 ? 0.9f : bagA));
        g.strokePath (p, PathStrokeType (4.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void Visualizer::drawMouth (Graphics& g)
{
    const float W = d[Mods::weed], C = d[Mods::coke], K = d[Mods::keta], L = d[Mods::lsd], M = d[Mods::mdma];
    const float sum = jmax (1.0f, W + C + K + L + M);
    auto blend = [&] (float base, float tw, float tc, float tk, float tl, float tm)
    {
        return base + (W * (tw - base) + C * (tc - base) + K * (tk - base) + L * (tl - base) + M * (tm - base)) / sum;
    };
    float ws  = blend (1.0f, 1.08f, 1.12f, 0.55f, 0.5f, 1.25f);
    float sag = blend (59.0f, 72.0f, 26.0f, 6.0f, 18.0f, 98.0f) + 16.0f * cl;
    float open = blend (0.0f, 30.0f, 44.0f, 22.0f, 100.0f, 62.0f);
    float teeth = blend (0.0f, 0.35f, 1.0f, 0.0f, 0.0f, 0.6f);
    float teethL = blend (0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.3f);
    float rnd01 = blend (0.0f, 0.0f, 0.0f, 0.55f, 1.0f, 0.0f);
    const float thick = 36.0f;

    open += C * 4.0f * std::sin (t * 23.0f) + M * 4.0f * std::sin (t * 17.0f);
    open = jmax (0.0f, open);

    const Point<float> mid { 1547.0f, 791.0f };
    Point<float> PL { 1345.0f, 810.0f + W * 12.0f }, PR { 1750.0f, 772.0f };
    PL = mid + (PL - mid) * ws; PR = mid + (PR - mid) * ws;
    if (C > 0.0f)
    {
        const float j = C * (1.0f - 0.8f * cl);
        PL += Point<float> ((rnd.nextFloat() - 0.5f) * 6.0f * j, (rnd.nextFloat() - 0.5f) * 4.0f * j);
        PR += Point<float> ((rnd.nextFloat() - 0.5f) * 6.0f * j, (rnd.nextFloat() - 0.5f) * 4.0f * j);
    }
    const Point<float> ctrl { (PL.x + PR.x) * 0.5f, (PL.y + PR.y) * 0.5f + 2.0f * sag };
    const float open01 = clamp01 (open / 12.0f);

    const int N = 40;
    std::vector<Point<float>> up ((size_t) N + 1), lo ((size_t) N + 1);
    const Point<float> uc = quad (PL, ctrl, PR, 0.5f);
    const float ea = 0.5f * PL.getDistanceFrom (PR), eb = jmax (open * 0.5f, 8.0f);
    const Point<float> ec { uc.x, uc.y + open * 0.4f };
    for (int i = 0; i <= N; ++i)
    {
        const float u = (float) i / (float) N;
        const float s = std::sin (u * 3.14159f);
        Point<float> cu = quad (PL, ctrl, PR, u);
        Point<float> cl = cu + Point<float> (0.0f, (thick * (1.0f - open01) + open) * std::pow (s, 0.8f));
        Point<float> eu { ec.x - ea * std::cos (u * 3.14159f), ec.y - eb * s };
        Point<float> el { ec.x - ea * std::cos (u * 3.14159f), ec.y + eb * s };
        up[(size_t) i] = P (lerp (cu.x, eu.x, rnd01), lerp (cu.y, eu.y, rnd01));
        lo[(size_t) i] = P (lerp (cl.x, el.x, rnd01), lerp (cl.y, el.y, rnd01));
    }

    Path mouth;
    mouth.startNewSubPath (up[0]);
    for (int i = 1; i <= N; ++i) mouth.lineTo (up[(size_t) i]);
    for (int i = N; i >= 0; --i) mouth.lineTo (lo[(size_t) i]);
    mouth.closeSubPath();

    if (open01 < 0.05f)
    {
        g.setColour (mouthCol); g.fillPath (mouth);
        Path lower; lower.startNewSubPath (lo[2]);
        for (int i = 3; i <= N - 2; ++i) lower.lineTo (lo[(size_t) i]);
        g.setColour (Colour (0xff4e2540));
        g.strokePath (lower, PathStrokeType (4.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }

    // lèvre basse
    Path lowLip; lowLip.startNewSubPath (lo[0]);
    for (int i = 1; i <= N; ++i) lowLip.lineTo (lo[(size_t) i]);
    g.setColour (mouthCol);
    g.strokePath (lowLip, PathStrokeType (14.0f * k, PathStrokeType::curved, PathStrokeType::rounded));

    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (mouth);
        g.setColour (Colour (0xff2a0a1d)); g.fillPath (mouth);

        // langue
        const auto bb = mouth.getBounds();
        g.setColour (Colour (0xffc75a80).withAlpha (clamp01 (open / 40.0f)));
        g.fillEllipse (bb.getCentreX() - bb.getWidth() * 0.28f, bb.getBottom() - bb.getHeight() * 0.38f,
                       bb.getWidth() * 0.56f, bb.getHeight() * 0.5f);

        auto teethBand = [&] (bool upper, float frac)
        {
            if (frac < 0.02f) return;
            Path tp;
            const float hgt = frac * open * k * (upper ? 0.55f : 0.5f);
            const auto& edge = upper ? up : lo;
            tp.startNewSubPath (edge[0]);
            for (int i = 1; i <= N; ++i) tp.lineTo (edge[(size_t) i]);
            for (int i = N; i >= 0; --i) tp.lineTo (edge[(size_t) i] + Point<float> (0.0f, upper ? hgt : -hgt));
            tp.closeSubPath();
            g.setColour (Colour (0xfff4f1e8)); g.fillPath (tp);
            g.setColour (Colour (0xff8f8a84));
            for (int i = 1; i < 10; ++i)
            {
                const auto e = edge[(size_t) (i * N / 10)];
                g.drawLine ({ e, e + Point<float> (0.0f, upper ? hgt : -hgt) }, 2.0f * k * 1.5f);
            }
        };
        teethBand (true, teeth);
        teethBand (false, teethL);
    }
    g.setColour (ink);
    g.strokePath (mouth, PathStrokeType (6.0f * k, PathStrokeType::curved, PathStrokeType::rounded));
}

void Visualizer::drawFace (Graphics& g)
{
    const float W = d[Mods::weed], C = d[Mods::coke], K = d[Mods::keta], L = d[Mods::lsd], M = d[Mods::mdma];

    // joues roses
    const float blush = clamp01 (0.6f * M + 0.25f * W + 0.15f * L + 0.4f * cl);
    if (blush > 0.01f)
        for (auto c : { P (1330, 840), P (1765, 805) })
        {
            ColourGradient gr (Colour (0xffff6aa8).withAlpha (0.6f * blush), c.x, c.y,
                               Colour (0xffff6aa8).withAlpha (0.0f), c.x + 75.0f * k, c.y, true);
            g.setGradientFill (gr);
            g.fillEllipse (Rectangle<float> (150.0f * k, 95.0f * k).withCentre (c));
        }

    // clignement
    const float bp = std::fmod (t, 4.3f);
    const float blink = (C < 0.3f && bp > 4.15f) ? std::sin ((bp - 4.15f) / 0.15f * 3.14159f) : 0.0f;

    const float scale = 1.0f + 0.1f * C + 0.04f * K;
    for (int side = 0; side < 2; ++side)
    {
        const float wob = L * 0.06f * std::sin (t * 3.0f + (float) side * 1.7f);
        const float cx = side == 0 ? 1426.0f : 1644.0f, cy = side == 0 ? 676.0f : 678.0f;
        const float rx = (side == 0 ? 101.0f : 99.0f) * (scale + wob), ry = 79.0f * (scale - wob);
        float lid = 0.38f + 0.42f * W - 0.33f * C - 0.22f * K - 0.3f * L - 0.08f * M + 0.06f * cl;
        lid += side == 0 ? 0.14f * K : -0.04f * K;
        lid = jlimit (0.02f, 0.9f, lid);
        lid = lerp (lid, 1.0f, blink);
        drawEye (g, cx * k, cy * k, rx * k, ry * k, lid, lookX, side);
    }

    // rides
    const float crow = clamp01 (0.9f * C + 0.5f * M + 0.3f * K) * (1.0f - 0.8f * cl);
    if (crow > 0.01f)
    {
        g.setColour (ink.withAlpha (crow));
        for (int side = 0; side < 2; ++side)
        {
            const float ex = side == 0 ? 1310.0f : 1758.0f, dir = side == 0 ? -1.0f : 1.0f;
            for (int i = -1; i <= 1; ++i)
                g.drawLine ({ P (ex, 676.0f + (float) i * 22.0f), P (ex + dir * 38.0f, 676.0f + (float) i * 38.0f) }, 4.0f * k);
        }
    }
    const float frown = clamp01 (0.95f * C + 0.3f * K) * (1.0f - 0.85f * cl);
    if (frown > 0.01f)
    {
        g.setColour (ink.withAlpha (frown));
        g.drawLine ({ P (1528, 640), P (1522, 700) }, 4.0f * k);
        g.drawLine ({ P (1548, 640), P (1554, 700) }, 4.0f * k);
    }
    const float smileLines = clamp01 (0.85f * M + 0.35f * W);
    if (smileLines > 0.01f)
    {
        g.setColour (ink.withAlpha (smileLines * 0.8f));
        Path a; a.startNewSubPath (P (1320, 770)); a.quadraticTo (P (1285, 820), P (1305, 880));
        Path b; b.startNewSubPath (P (1772, 735)); b.quadraticTo (P (1815, 785), P (1800, 850));
        g.strokePath (a, PathStrokeType (4.5f * k, PathStrokeType::curved, PathStrokeType::rounded));
        g.strokePath (b, PathStrokeType (4.5f * k, PathStrokeType::curved, PathStrokeType::rounded));
    }

    drawMouth (g);

    // gouttes de sueur
    const float sweat = clamp01 (0.95f * C + 0.5f * M) * (1.0f - 0.8f * cl);
    if (sweat > 0.01f)
    {
        static const float drops[4][3] = { { 1290, 610, 0.0f }, { 1800, 600, 0.4f }, { 1330, 560, 0.7f }, { 1760, 650, 0.2f } };
        for (auto& dr : drops)
        {
            const float ph = std::fmod (t * (0.4f + C * 0.6f) + dr[2], 1.0f);
            const auto c = P (dr[0], dr[1] + ph * 160.0f);
            const float s = 18.0f * k;
            Path p; p.startNewSubPath (c.x, c.y - s * 1.6f);
            p.quadraticTo (c.x + s, c.y, c.x, c.y + s);
            p.quadraticTo (c.x - s, c.y, c.x, c.y - s * 1.6f);
            const float a = sweat * std::sin (ph * 3.14159f);
            g.setColour (Colour (0xffbfeaff).withAlpha (a)); g.fillPath (p);
            g.setColour (ink.withAlpha (a)); g.strokePath (p, PathStrokeType (2.5f * k));
        }
    }
}

void Visualizer::drawOverlays (Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float W = d[Mods::weed], C = d[Mods::coke], K = d[Mods::keta], L = d[Mods::lsd], M = d[Mods::mdma];
    const float hx = w * 0.502f, hy = h * 0.47f;

    if (W > 0.01f)
    {
        ColourGradient gr (Colour (0xff6fcf7f).withAlpha (0.0f), 0, 0, Colour (0xff6fcf7f).withAlpha (0.22f * W), 0, h, false);
        g.setGradientFill (gr); g.fillAll();
    }
    if (L > 0.01f) // pétales kaléidoscope autour de la tête
    {
        const int petals = 14;
        for (int i = 0; i < petals; ++i)
        {
            const float a = (float) i / (float) petals * 6.283f + t * 0.4f;
            const float r = h * (0.62f + 0.06f * std::sin (t * 2.0f + (float) i));
            Path p; p.addEllipse (-h * 0.05f, -h * 0.16f, h * 0.1f, h * 0.32f);
            p.applyTransform (AffineTransform::rotation (a + 1.5708f).translated (hx + std::cos (a) * r, hy + std::sin (a) * r * 0.75f));
            g.setColour (Colour::fromHSV (std::fmod ((float) i / (float) petals + t * 0.1f, 1.0f), 0.8f, 1.0f, 0.22f * L));
            g.fillPath (p);
        }
        g.setColour (Colour::fromHSV (std::fmod (t * 0.07f, 1.0f), 0.7f, 1.0f, 0.08f * L));
        g.fillAll();
    }
    if (K > 0.01f)
    {
        g.setColour (Colour (0xffdfe4ee).withAlpha (0.2f * K));
        g.fillAll();
    }
    if (C > 0.01f)
    {
        g.setColour (Colour (0xffbfeaff).withAlpha (0.08f * C * jmin (1.0f, level * 2.0f)));
        g.fillAll();
        g.setColour (Colour (0xffd8f3ff).withAlpha (0.8f * C));
        for (int side = 0; side < 2; ++side)
            for (int i = 0; i < 4; ++i)
            {
                if (rnd.nextFloat() < 0.3f) continue;
                const float x = side == 0 ? w * 0.24f : w * 0.75f, y = h * (0.28f + 0.1f * (float) i);
                const float len = w * 0.05f * (0.6f + rnd.nextFloat() * 0.6f);
                g.drawLine (x - (side == 0 ? len : 0.0f), y, x + (side == 0 ? 0.0f : len), y, 2.0f);
            }
    }
    if (M > 0.01f)
    {
        g.setColour (Colour (0xffff7eb9).withAlpha (0.12f * M * std::pow (1.0f - beatPh, 2.0f)));
        g.fillAll();
    }

    for (const auto& p : parts)
    {
        const float fade = jmin (1.0f, p.life / 0.5f) * jmin (1.0f, (p.maxLife - p.life) / 0.4f);
        switch (p.type)
        {
            case 0: g.setColour (Colour (0xff9ad8a4).withAlpha (0.16f * fade));
                    g.fillEllipse (Rectangle<float> (p.size * 2, p.size * 1.6f).withCentre ({ p.x, p.y })); break;
            case 1: g.setColour (Colour (0xffc8efff).withAlpha (fade));
                    g.drawLine (p.x, p.y, p.x - p.vx * 0.05f, p.y - p.vy * 0.05f, 2.0f); break;
            case 2: g.setColour (Colour (0xffe6eaf2).withAlpha (0.08f * fade));
                    g.fillEllipse (Rectangle<float> (p.size * 2, p.size).withCentre ({ p.x, p.y })); break;
            case 3: g.setColour (Colour::fromHSV (p.hue, 0.8f, 1.0f, 0.85f * fade));
                    g.fillEllipse (Rectangle<float> (p.size, p.size).withCentre ({ p.x, p.y })); break;
            case 4:
            {
                auto hp = heartPath (p.x, p.y, p.size);
                g.setColour (Colour (0xffff5fae).withAlpha (0.85f * fade)); g.fillPath (hp);
                g.setColour (ink.withAlpha (0.6f * fade)); g.strokePath (hp, PathStrokeType (1.5f));
                break;
            }
            case 5:
            {
                g.setColour (Colours::white.withAlpha (0.9f * fade));
                const float s = p.size * (0.5f + 0.5f * std::sin (p.rot * 3.0f));
                g.drawLine (p.x - s, p.y, p.x + s, p.y, 1.5f);
                g.drawLine (p.x, p.y - s, p.x, p.y + s, 1.5f);
                break;
            }
            case 6:
            {
                const float r = p.size;
                ColourGradient gr (Colour (0xffffd9a0).withAlpha (0.35f * fade), p.x, p.y, Colour (0xffffd9a0).withAlpha (0.0f), p.x + r, p.y, true);
                g.setGradientFill (gr);
                g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre ({ p.x, p.y }));
                break;
            }
            default: break;
        }
    }

    if (cl > 0.01f) // CLEAN : lumière chaude et douce
    {
        ColourGradient warmG (Colour (0xffffc27a).withAlpha (0.24f * cl), hx, hy, Colour (0xffffc27a).withAlpha (0.0f), hx + w * 0.55f, hy, true);
        g.setGradientFill (warmG); g.fillAll();
        g.setColour (Colour (0xffffb36b).withAlpha (0.07f * cl)); g.fillAll();
        ColourGradient vg (Colours::transparentBlack, hx, hy, Colour (0xff2a1408).withAlpha (0.45f * cl), hx + w * 0.7f, hy, true);
        vg.addColour (0.55f, Colours::transparentBlack);
        g.setGradientFill (vg); g.fillAll();
    }

    if (K > 0.01f) // tunnel K-hole
    {
        ColourGradient vg (Colours::transparentBlack, hx, hy, Colours::black.withAlpha (0.85f * K), hx + w * (0.75f - 0.3f * K), hy, true);
        vg.addColour (0.45f, Colours::transparentBlack);
        g.setGradientFill (vg); g.fillAll();
    }
}

//==============================================================================
ModuleStrip::ModuleStrip (AudioProcessorValueTreeState& apvts, int i)
    : index (i), colour (Palette::mod (i)),
      powerAtt (apvts, String (Mods::key (i)) + "_on", power),
      doseAtt (apvts, String (Mods::key (i)) + "_dose", dose),
      mixAtt (apvts, String (Mods::key (i)) + "_mix", mix),
      advAtt (apvts, String (Mods::key (i)) + "_adv", adv),
      adv2Att (apvts, String (Mods::key (i)) + "_adv2", adv2)
{
    power.setColour (ToggleButton::tickColourId, colour);
    power.onStateChange = [this] { repaint(); };
    addAndMakeVisible (power);

    dose.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    dose.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    dose.setColour (Slider::rotarySliderFillColourId, colour);
    dose.onValueChange = [this] { repaint(); };
    addAndMakeVisible (dose);

    mix.setSliderStyle (Slider::LinearVertical);
    mix.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    mix.setColour (Slider::trackColourId, colour);
    mix.onValueChange = [this] { repaint(); };
    addAndMakeVisible (mix);

    adv.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    adv.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    adv.setColour (Slider::rotarySliderFillColourId, colour.withAlpha (0.8f));
    adv.onValueChange = [this] { repaint(); };
    addAndMakeVisible (adv);
    adv2.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    adv2.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    adv2.setColour (Slider::rotarySliderFillColourId, colour.withAlpha (0.8f));
    adv2.onValueChange = [this] { repaint(); };
    addAndMakeVisible (adv2);
}

void ModuleStrip::update (float effectLevel)
{
    const float nl = level + (effectLevel - level) * 0.4f;
    const String f = ! power.getToggleState() ? U ("prescription only")
                   : U (feelings[index][jmin (2, (int) (dose.getValue() / 33.34))]);
    if (std::abs (nl - level) > 0.004f || f != feeling)
    {
        level = nl; feeling = f;
        repaint (0, 206, getWidth(), getHeight() - 206);
    }
}

void ModuleStrip::paint (Graphics& g)
{
    const bool on = power.getToggleState();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (on ? colour.withAlpha (0.9f) : Palette::line);
    g.drawRoundedRectangle (r, 10.0f, on ? 1.5f : 1.0f);
    if (on)
    {
        g.setColour (colour.withAlpha (0.08f));
        g.fillRoundedRectangle (r.reduced (2.0f), 9.0f);
    }

    g.setColour (on ? colour : Palette::fg);
    g.setFont (FontOptions (21.0f, Font::bold));
    g.drawText (Mods::label (index), 12, 8, getWidth() - 80, 24, Justification::centredLeft);
    g.setColour (Palette::muted);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText (Mods::tool (index), 12, 31, getWidth() - 80, 12, Justification::centredLeft);

    // étiquettes
    g.setFont (FontOptions (10.0f, Font::bold));
    g.setColour (Palette::muted);
    const auto db = dose.getBounds();
    g.drawText (U ("DOSE ") + String (roundToInt (dose.getValue())) + " %", db.getX(), db.getBottom() - 2, db.getWidth(), 14, Justification::centred);
    const auto mb = mix.getBounds();
    g.drawText ("MIX", mb.getX() - 6, mb.getBottom() + 2, mb.getWidth() + 12, 12, Justification::centred);
    for (int w = 0; w < 2; ++w)
    {
        const auto& kn = w == 0 ? adv : adv2;
        const auto ab = kn.getBounds();
        const int tw = getWidth() / 2 - ab.getWidth() - 14;
        g.setColour (Palette::muted);
        g.setFont (FontOptions (9.5f, Font::bold));
        g.drawText (Mods::advName (index, w), ab.getRight() + 3, ab.getY() + 5, tw, 11, Justification::centredLeft);
        g.setColour (Palette::fg);
        g.setFont (FontOptions (11.0f));
        g.drawFittedText (Mods::advText (index, w, (float) kn.getValue() / 100.0f), ab.getRight() + 3, ab.getY() + 18, tw, 14, Justification::centredLeft, 1, 0.8f);
    }

    // niveau d'effet
    const float mx = 12.0f, my = 230.0f, mw = (float) getWidth() - 24.0f - 34.0f, mh = 10.0f;
    const int segs = 14;
    const float sw = (mw - (float) (segs - 1) * 2.0f) / (float) segs;
    const int lit = roundToInt (level * (float) segs);
    for (int s = 0; s < segs; ++s)
    {
        g.setColour (s < lit ? colour.withAlpha (0.45f + 0.55f * (float) s / (float) segs) : Palette::line);
        g.fillRoundedRectangle (mx + (float) s * (sw + 2.0f), my, sw, mh, 2.0f);
    }
    g.setColour (Palette::fg);
    g.setFont (FontOptions (11.0f));
    g.drawText (String (roundToInt (level * 100.0f)) + "%", (int) (mx + mw + 2.0f), (int) my - 3, 34, 16, Justification::centredRight);
    g.setColour (Palette::muted);
    g.setFont (FontOptions (9.5f, Font::bold));
    g.drawText (U ("EFFECT LEVEL"), (int) mx, (int) my - 15, 140, 12, Justification::centredLeft);

    // case sensation
    auto box = Rectangle<float> (12.0f, 250.0f, (float) getWidth() - 24.0f, (float) getHeight() - 260.0f);
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (on ? colour.withAlpha (0.5f) : Palette::line);
    g.drawRoundedRectangle (box, 6.0f, 1.0f);
    g.setColour (on ? colour : Palette::muted.withAlpha (0.7f));
    g.setFont (FontOptions (13.5f, on ? Font::bold : Font::italic));
    g.drawFittedText ((on ? U ("“") : String()) + feeling + (on ? U ("”") : String()), box.reduced (6.0f, 2.0f).toNearestInt(),
                      Justification::centred, 2, 0.85f);
}

void ModuleStrip::resized()
{
    power.setBounds (getWidth() - 12 - 60, 11, 60, 24);
    dose.setBounds (10, 46, 104, 104);
    mix.setBounds (getWidth() - 44, 52, 26, 94);
    adv.setBounds (8, 164, 38, 38);
    adv2.setBounds (getWidth() / 2 + 2, 164, 38, 38);
}

//==============================================================================
void PharmacieEditor::Content::paint (Graphics& g)
{
    g.fillAll (Palette::bg);
    g.setColour (Palette::muted);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (U ("MARY  ·  PHARMACY  V4"), 16, baseH - 24, 300, 18, Justification::centredLeft);
}

PharmacieEditor::PharmacieEditor (PharmacieAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseW, baseH);
    content.addAndMakeVisible (visual);

    for (int i = 0; i < Mods::count; ++i)
    {
        strips[(size_t) i] = std::make_unique<ModuleStrip> (p.apvts, i);
        content.addAndMakeVisible (*strips[(size_t) i]);
    }

    auto setupBar = [this] (Slider& s, Label& l, const String& text)
    {
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setTextBoxStyle (Slider::TextBoxRight, false, 64, 20);
        s.setColour (Slider::trackColourId, Palette::neon);
        s.setColour (Slider::textBoxOutlineColourId, Palette::line);
        content.addAndMakeVisible (s);
        l.setText (text, dontSendNotification);
        l.setFont (FontOptions (11.0f, Font::bold));
        l.setColour (Label::textColourId, Palette::muted);
        l.setJustificationType (Justification::centredRight);
        content.addAndMakeVisible (l);
    };
    setupBar (mixSlider, mixLabel, "MIX");
    setupBar (outSlider, outLabel, "OUTPUT");
    mixAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "mix", mixSlider);
    outAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "out", outSlider);
    cleanSlider.setSliderStyle (Slider::LinearHorizontal);
    cleanSlider.setTextBoxStyle (Slider::TextBoxRight, false, 64, 20);
    cleanSlider.setColour (Slider::trackColourId, Colour (0xffffc27a));
    cleanSlider.setColour (Slider::textBoxOutlineColourId, Palette::line);
    content.addAndMakeVisible (cleanSlider);
    cleanBtn.setButtonText ("CLEAN");
    cleanBtn.setColour (ToggleButton::tickColourId, Colour (0xffffc27a));
    content.addAndMakeVisible (cleanBtn);
    cleanAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "clean", cleanSlider);
    cleanBtnAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "clean_on", cleanBtn);

    visual.setBounds (0, 0, baseW, 480);
    const int pad = 12, gap = 10, sy = 480 + 12, sh = 300;
    const int sw = (baseW - 2 * pad - 4 * gap) / 5;
    for (int i = 0; i < Mods::count; ++i)
        strips[(size_t) i]->setBounds (pad + i * (sw + gap), sy, sw, sh);
    const int fy = sy + sh + 14;
    cleanBtn.setBounds (pad, fy + 2, 104, 32);
    cleanSlider.setBounds (pad + 114, fy + 2, 210, 32);
    outSlider.setBounds (baseW - pad - 210, fy + 2, 210, 32);
    outLabel.setBounds (baseW - pad - 210 - 64, fy + 2, 60, 32);
    mixSlider.setBounds (baseW - pad - 210 - 74 - 210, fy + 2, 210, 32);
    mixLabel.setBounds (baseW - pad - 210 - 74 - 210 - 44, fy + 2, 40, 32);

    setResizable (true, true);
    setResizeLimits (baseW * 2 / 3, baseH * 2 / 3, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);
    startTimerHz (30);
}

PharmacieEditor::~PharmacieEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PharmacieEditor::paint (Graphics& g) { g.fillAll (Palette::bg); }

void PharmacieEditor::resized()
{
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) baseW));
}

void PharmacieEditor::timerCallback()
{
    float levels[Mods::count];
    for (int i = 0; i < Mods::count; ++i)
    {
        const String key (Mods::key (i));
        const bool on = proc.apvts.getRawParameterValue (key + "_on")->load() > 0.5f;
        const float target = on ? proc.apvts.getRawParameterValue (key + "_dose")->load() / 100.0f : 0.0f;
        visDose[i] += (target - visDose[i]) * 0.12f;
        levels[i] = proc.effectLevel[(size_t) i].load();
        strips[(size_t) i]->update (levels[i]);
    }
    const float ct = proc.apvts.getRawParameterValue ("clean_on")->load() > 0.5f ? proc.apvts.getRawParameterValue ("clean")->load() / 100.0f : 0.0f;
    visClean += (ct - visClean) * 0.08f;
    visual.tick (visDose, levels, proc.outputLevel.load(), proc.beatPhase.load(), visClean);
}
