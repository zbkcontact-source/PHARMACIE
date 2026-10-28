#include "PluginEditor.h"

using namespace juce;

static String U (const char* s) { return String::fromUTF8 (s); }

//==============================================================================
PharmacieLookAndFeel::PharmacieLookAndFeel()
{
    setColour (Label::textColourId, Palette::fg);
    setColour (Slider::textBoxTextColourId, Palette::fg);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
}

void PharmacieLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                             float a0, float a1, Slider& s)
{
    const auto col = s.findColour (Slider::rotarySliderFillColourId);
    const auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float arcR = radius - 5.0f;
    const float ang = a0 + pos * (a1 - a0);

    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (Palette::line);
    g.strokePath (track, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        Path fill;
        fill.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);
        g.setColour (s.isEnabled() ? col : col.withAlpha (0.35f));
        g.strokePath (fill, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float knobR = radius * 0.64f;
    g.setColour (Palette::panel2);
    g.fillEllipse (Rectangle<float> (knobR * 2, knobR * 2).withCentre (c));
    g.setColour (Palette::line);
    g.drawEllipse (Rectangle<float> (knobR * 2, knobR * 2).withCentre (c), 1.0f);

    const Point<float> tip = c.getPointOnCircumference (knobR * 0.82f, ang);
    const Point<float> from = c.getPointOnCircumference (knobR * 0.25f, ang);
    g.setColour (Palette::fg);
    g.drawLine ({ from, tip }, 2.5f);
}

void PharmacieLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                             Slider::SliderStyle, Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const auto col = s.findColour (Slider::trackColourId);
    g.setColour (Palette::line);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (col);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, pos - (float) x, 4.0f, 2.0f);
    g.setColour (Palette::fg);
    g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

void PharmacieLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    const auto col = b.findColour (ToggleButton::tickColourId);
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Palette::panel2);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? col : (highlighted ? Palette::muted : Palette::line));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);

    const auto led = Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 16.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (col.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (on ? col : Palette::line);
    g.fillEllipse (led);

    g.setColour (Palette::fg);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText (on ? "ON" : "OFF", r.withTrimmedLeft (26.0f), Justification::centredLeft);
}

//==============================================================================
ModulePanel::ModulePanel (AudioProcessorValueTreeState& apvts, const String& id, const String& n,
                          Colour c, const String& desc, StringArray m)
    : colour (c), name (n), description (desc), moods (std::move (m)),
      powerAtt (apvts, id + "_on", power),
      knobAtt (apvts, id + "_dose", knob)
{
    power.setColour (ToggleButton::tickColourId, colour);
    power.setTooltip ("Activer " + name);
    power.onStateChange = [this] { refresh(); };
    addAndMakeVisible (power);

    knob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 20);
    knob.setColour (Slider::rotarySliderFillColourId, colour);
    knob.onValueChange = [this] { refresh(); };
    addAndMakeVisible (knob);

    mood.setJustificationType (Justification::centred);
    mood.setFont (FontOptions (12.5f, Font::italic));
    addAndMakeVisible (mood);
    refresh();
}

float ModulePanel::effectiveDose() const
{
    return power.getToggleState() ? (float) knob.getValue() / 100.0f : 0.0f;
}

void ModulePanel::refresh()
{
    const bool on = power.getToggleState();
    const int idx = jmin (3, (int) (knob.getValue() / 25.0001));
    mood.setText (moods[idx], dontSendNotification);
    mood.setColour (Label::textColourId, on ? colour : Palette::muted.withAlpha (0.7f));
    knob.setColour (Slider::rotarySliderFillColourId, on ? colour : colour.withAlpha (0.4f));
    if (on != lastOn) { lastOn = on; repaint(); }
}

void ModulePanel::paint (Graphics& g)
{
    const bool on = power.getToggleState();
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    if (on)
    {
        g.setColour (colour.withAlpha (0.12f));
        g.fillRoundedRectangle (r.expanded (1.0f), 15.0f);
    }
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (on ? colour : Palette::line);
    g.drawRoundedRectangle (r, 14.0f, on ? 1.5f : 1.0f);

    g.setColour (on ? colour : Palette::fg);
    g.setFont (FontOptions (20.0f, Font::bold));
    g.drawText (name, 16, 14, getWidth() - 32, 26, Justification::centredLeft);

    g.setColour (Palette::muted);
    g.setFont (FontOptions (12.0f));
    g.drawFittedText (description, 16, 50, getWidth() - 32, 48, Justification::topLeft, 4, 1.0f);
}

void ModulePanel::resized()
{
    auto r = getLocalBounds();
    power.setBounds (r.getWidth() - 16 - 66, 14, 66, 26);
    knob.setBounds (Rectangle<int> (120, 132).withCentre ({ r.getCentreX(), 166 }));
    mood.setBounds (8, r.getHeight() - 36, r.getWidth() - 16, 22);
}

//==============================================================================
PharmacieEditor::PharmacieEditor (PharmacieAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    modules[0] = std::make_unique<ModulePanel> (p.apvts, "lsd", "LSD", Palette::lsd,
        U ("Chorus flottant, flanger qui boucle, panoramique qui tourne, réverb immense."),
        StringArray { "les couleurs vibrent", "les murs respirent", "je parle aux nuages", U ("dissous dans l’univers") });
    modules[1] = std::make_unique<ModulePanel> (p.apvts, "coke", U ("Cocaïne"), Palette::coke,
        U ("Pitch qui monte, compression écrasée, aigus agressifs, trémolo nerveux."),
        StringArray { "un peu speed", "je parle trop vite", U ("14 idées de start-up"), U ("cœur à 300 bpm") });
    modules[2] = std::make_unique<ModulePanel> (p.apvts, "weed", "Weed", Palette::weed,
        U ("Pitch qui descend, son étouffé, bande qui pleure, réverb cotonneuse."),
        StringArray { U ("détendu"), "tout est lent", U ("le canapé m’aspire"), U ("j’ai oublié la question") });
    modules[3] = std::make_unique<ModulePanel> (p.apvts, "acid", "Acide", Palette::acid,
        U ("Filtre résonant qui squelche façon 303, saturation, grain numérique, anneau."),
        StringArray { "squelch discret", U ("la 303 se réveille"), U ("rave en entrepôt"), U ("résonance infinie") });

    for (auto& m : modules) addAndMakeVisible (*m);

    auto setupBar = [this] (Slider& s, Label& l, const String& text)
    {
        s.setSliderStyle (Slider::LinearHorizontal);
        s.setTextBoxStyle (Slider::TextBoxRight, false, 70, 20);
        s.setColour (Slider::trackColourId, Palette::fg);
        addAndMakeVisible (s);
        l.setText (text, dontSendNotification);
        l.setFont (FontOptions (11.0f, Font::bold));
        l.setColour (Label::textColourId, Palette::muted);
        addAndMakeVisible (l);
    };
    setupBar (mixSlider, mixLabel, "MIX");
    setupBar (outSlider, outLabel, "SORTIE");
    mixAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "mix", mixSlider);
    outAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "out", outSlider);

    setSize (860, 520);
    startTimerHz (30);
}

PharmacieEditor::~PharmacieEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PharmacieEditor::timerCallback()
{
    t += 1.0 / 30.0;
    level += (proc.outputLevel.load() - level) * 0.35f;
    for (auto& m : modules) m->refresh();
    repaint (visualArea);
}

void PharmacieEditor::paint (Graphics& g)
{
    g.fillAll (Palette::bg);

    g.setColour (Palette::muted);
    g.setFont (FontOptions (10.5f, Font::bold));
    g.drawText (U ("RACK D’EFFETS  ·  4 MODULES  ·  MARY"), 20, 14, 400, 16, Justification::centredLeft);
    g.setColour (Palette::fg);
    g.setFont (FontOptions (28.0f, Font::bold));
    g.drawText ("PHARMACIE", 20, 30, 500, 34, Justification::centredLeft);

    // Etat courant
    StringArray active;
    const String names[] = { "LSD", U ("Cocaïne"), "Weed", "Acide" };
    for (int i = 0; i < 4; ++i)
        if (modules[(size_t) i]->effectiveDose() > 0.0f)
            active.add (String (names[i]) + " " + String (roundToInt (modules[(size_t) i]->effectiveDose() * 100.0f)));
    g.setColour (Palette::muted);
    g.setFont (FontOptions (12.0f));
    g.drawText (U ("État : ") + (active.isEmpty() ? String ("sobre") : active.joinIntoString (U ("  ·  "))),
                getWidth() - 420, 40, 400, 20, Justification::centredRight);

    paintVisual (g, visualArea.toFloat());
}

void PharmacieEditor::paintVisual (Graphics& g, Rectangle<float> area)
{
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (area, 12.0f);
    g.setColour (Palette::line);
    g.drawRoundedRectangle (area, 12.0f, 1.0f);

    const float L = modules[0]->effectiveDose(), C = modules[1]->effectiveDose();
    const float W = modules[2]->effectiveDose(), A = modules[3]->effectiveDose();

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (area.toNearestInt());

    // Couleur : mélange pondéré des modules actifs
    Colour col = Palette::muted;
    const float tot = L + C + W + A;
    if (tot > 0.0f)
    {
        col = Palette::lsd.withAlpha (1.0f);
        float acc = L;
        auto blend = [&] (Colour c, float d) { if (d > 0.0f) { acc += d; col = col.interpolatedWith (c, d / acc); } };
        if (L <= 0.0f) { col = Palette::coke; acc = 0.0f; }
        blend (Palette::coke, C);
        blend (Palette::weed, W);
        blend (Palette::acid, A);
    }

    const auto c = area.getCentre();
    const float shake = C * 3.0f;
    const float speed = 1.0f + L * 4.0f + C * 3.0f - W * 0.7f;
    const int layers = 1 + roundToInt (L * 6.0f);
    const float R = area.getHeight() * 0.28f;
    const float steps = A > 0.0f ? 40.0f - A * 34.0f : 0.0f;
    Random& rnd = Random::getSystemRandom();

    for (int k = 0; k < layers; ++k)
    {
        Path p;
        const float sc = 1.0f + (float) k * 0.12f;
        const float rot = (float) t * 0.4f * speed + (float) k * MathConstants<float>::pi / (float) layers;
        for (int i = 0; i <= 160; ++i)
        {
            const float a = (float) i / 160.0f * MathConstants<float>::twoPi;
            float v = std::sin (a * (3.0f + A * 9.0f) + rot * 2.0f) * (0.12f + level * 0.6f)
                    + std::sin (a * 7.0f - rot * 3.0f) * 0.05f * (1.0f + L * 3.0f);
            if (steps > 0.0f) v = std::round (v * steps) / steps;
            const float rr = R * sc * (1.0f + v);
            const float x = c.x + std::cos (a + rot * 0.3f) * rr * 2.6f + (rnd.nextFloat() - 0.5f) * shake;
            const float y = c.y + std::sin (a + rot * 0.3f) * rr;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        p.closeSubPath();
        const Colour lc = col.withRotatedHue ((float) k * 0.08f * L + (float) t * 0.05f * L)
                             .withAlpha (jmax (0.2f, 0.9f - (float) k * 0.1f));
        g.setColour (lc);
        g.strokePath (p, PathStrokeType (1.4f + C * 2.0f + A * 1.2f));
        if (W > 0.0f)
        {
            g.setColour (lc.withAlpha (0.12f * W));
            g.strokePath (p, PathStrokeType (8.0f + W * 10.0f));
        }
    }
}

void PharmacieEditor::resized()
{
    auto r = getLocalBounds().reduced (20, 0);
    r.removeFromTop (74);
    visualArea = r.removeFromTop (100);
    r.removeFromTop (14);

    auto footer = r.removeFromBottom (52);
    auto rack = r.withTrimmedBottom (8);
    const int gap = 12;
    const int w = (rack.getWidth() - gap * 3) / 4;
    for (size_t i = 0; i < modules.size(); ++i)
        modules[i]->setBounds (rack.getX() + (int) i * (w + gap), rack.getY(), w, rack.getHeight());

    footer = footer.withSizeKeepingCentre (footer.getWidth(), 30);
    auto left = footer.removeFromLeft (footer.getWidth() / 2 - 10);
    footer.removeFromLeft (20);
    mixLabel.setBounds (left.removeFromLeft (44));
    mixSlider.setBounds (left);
    outLabel.setBounds (footer.removeFromLeft (64));
    outSlider.setBounds (footer);
}
