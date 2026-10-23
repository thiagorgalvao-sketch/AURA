#include "PluginEditor.h"

namespace
{
    using namespace juce;

    constexpr float pi    = MathConstants<float>::pi;
    constexpr float twoPi = MathConstants<float>::twoPi;

    // Texto "gravado" no metal: realce claro 1 px abaixo + texto escuro
    void drawEngraved (Graphics& g, const String& text, Rectangle<float> area, Justification just,
                       float size, bool bold = true, float kerning = 0.12f,
                       Colour colour = AuraColours::engraving)
    {
        g.setFont (Font (FontOptions (size, bold ? Font::bold : Font::plain).withKerningFactor (kerning)));
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    void drawEngravedLine (Graphics& g, float x1, float x2, float y)
    {
        if (x2 - x1 < 4.0f) return;
        g.setColour (Colours::white.withAlpha (0.55f));
        g.fillRect (Rectangle<float> (x1, y + 1.0f, x2 - x1, 1.0f));
        g.setColour (AuraColours::engraving.withAlpha (0.7f));
        g.fillRect (Rectangle<float> (x1, y, x2 - x1, 1.0f));
    }

    void drawScrew (Graphics& g, Point<float> c, float r, float angle)
    {
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - r - 1, c.y - r, (r + 1) * 2, (r + 1) * 2 + 1);
        g.setGradientFill (ColourGradient (Colour (0xffe4e5e7), c.x, c.y - r, Colour (0xff8e9094), c.x, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (Colour (0xff55575b));
        for (float a : { angle, angle + pi * 0.5f })
            g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.65f, a),
                                     c.getPointOnCircumference (r * 0.65f, a + pi)), 1.6f);
    }

    double now() { return Time::getMillisecondCounterHiRes() * 0.001; }

    // Margem que o knob deixa livre dentro do componente (para a sombra)
    constexpr float knobInset = 14.0f;
}

//==============================================================================
// Knob
//==============================================================================
void AuraLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, Slider& slider)
{
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (knobInset);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float alpha = slider.isEnabled() ? 1.0f : 0.5f;

    // Escala de marcações gravada no chassi
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float len = (i == 0 || i == 5 || i == 10) ? 7.0f : 4.0f;
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawLine (Line<float> (c.getPointOnCircumference (r + 4.0f, a).translated (0.0f, 1.0f),
                                 c.getPointOnCircumference (r + 4.0f + len, a).translated (0.0f, 1.0f)), 1.2f);
        g.setColour (AuraColours::engraving.withAlpha (0.75f));
        g.drawLine (Line<float> (c.getPointOnCircumference (r + 4.0f, a),
                                 c.getPointOnCircumference (r + 4.0f + len, a)), 1.2f);
    }

    // Sombra no chassi
    for (int i = 0; i < 5; ++i)
    {
        const float rr = r + (float) i * 1.2f;
        g.setColour (Colours::black.withAlpha (0.06f));
        g.fillEllipse (c.x - rr, c.y - rr + 3.0f + (float) i * 0.7f, rr * 2.0f, rr * 2.0f);
    }

    // Saia serrilhada (gira com o knob)
    g.setGradientFill (ColourGradient (Colour (0xffc3c5c8), c.x, c.y - r, Colour (0xff66686c), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    constexpr int teeth = 80;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = angle + (float) i * twoPi / (float) teeth;
        const float light = 0.5f + 0.5f * std::cos (a);   // luz vindo de cima
        g.setColour ((i % 2 == 0 ? Colours::white : Colours::black)
                         .withAlpha ((i % 2 == 0 ? 0.18f + 0.35f * light : 0.30f) * alpha));
        g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.83f, a), c.getPointOnCircumference (r * 0.99f, a)), 1.3f);
    }
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);

    // Tampa usinada
    const float rc = r * 0.80f;
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - rc - 1.0f, c.y - rc, (rc + 1.0f) * 2.0f, (rc + 1.0f) * 2.0f);

    ColourGradient cap (Colour (0xffeceef0), c.x - rc * 0.35f, c.y - rc * 0.45f,
                        Colour (0xff8c8f93), c.x + rc * 0.7f, c.y + rc * 0.95f, true);
    cap.addColour (0.55, Colour (0xffc0c2c6));
    g.setGradientFill (cap);
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f);

    for (int k = 2; k <= 22; ++k)   // anéis do torneamento
    {
        const float rr = rc * (float) k / 22.0f;
        g.setColour ((k % 2 == 0 ? Colours::white : Colours::black).withAlpha (0.045f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.8f);
    }

    auto capRect = Rectangle<float> (rc * 2.0f, rc * 2.0f).withCentre (c);
    for (float base : { -0.25f * pi, 0.75f * pi })   // reflexo cônico fixo
    {
        Path sheen;
        sheen.addPieSegment (capRect, base - 0.22f, base + 0.22f, 0.0f);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.fillPath (sheen);
    }
    g.setColour (Colours::white.withAlpha (0.55f));
    g.drawEllipse (capRect.reduced (0.5f), 0.9f);

    // Indicador
    Path pointer;
    pointer.addRoundedRectangle (-1.7f, -rc * 0.93f, 3.4f, rc * 0.45f, 1.7f);
    g.setColour (Colour (0xff232427).withAlpha (alpha));
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
// Footswitch
//==============================================================================
Footswitch::Footswitch (const String& label, Colour ledColour)
    : ToggleButton (label), led (ledColour)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void Footswitch::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat();
    const bool on = getToggleState();

    // LED em cima, nome e botão embaixo
    auto ledArea = b.removeFromTop (30.0f);
    drawEngraved (g, getButtonText(), b.removeFromBottom (20.0f), Justification::centred, 13.0f);

    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.42f;

    for (int i = 0; i < 5; ++i)   // sombra
    {
        g.setColour (Colours::black.withAlpha (0.06f));
        const float rr = R + (float) i * 1.2f;
        g.fillEllipse (c.x - rr, c.y - rr + 2.0f + (float) i * 0.6f, rr * 2.0f, rr * 2.0f);
    }

    // Porca sextavada
    Path nut;
    nut.addPolygon (c, 6, R, pi / 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xffe1e2e4), c.x, c.y - R, Colour (0xff76787c), c.x, c.y + R, false));
    g.fillPath (nut);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.strokePath (nut, PathStrokeType (1.0f));

    // Arruela
    const float rw = R * 0.80f;
    g.setGradientFill (ColourGradient (Colour (0xff6e7074), c.x, c.y - rw, Colour (0xffd9dadc), c.x, c.y + rw, false));
    g.fillEllipse (c.x - rw, c.y - rw, rw * 2.0f, rw * 2.0f);

    // Botão (afunda ao pressionar)
    const float rb = R * (down ? 0.54f : 0.58f);
    ColourGradient dome (Colour (0xfffbfbfc), c.x - rb * 0.3f, c.y - rb * 0.4f,
                         Colour (0xff8d9094), c.x + rb * 0.6f, c.y + rb, true);
    dome.addColour (0.5, Colour (0xffd2d4d7));
    g.setGradientFill (dome);
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f, 1.0f);
    if (highlighted && ! down)
    {
        g.setColour (Colours::white.withAlpha (0.15f));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);
    }

    // LED
    const auto lc = ledArea.getCentre();
    const float lr = 6.5f;
    g.setColour (Colour (0xff2a2b2e));
    g.fillEllipse (lc.x - lr - 2.5f, lc.y - lr - 2.5f, (lr + 2.5f) * 2.0f, (lr + 2.5f) * 2.0f);
    if (on)
    {
        for (int i = 3; i >= 1; --i)
        {
            const float gr = lr + (float) i * 2.5f;
            g.setColour (led.withAlpha (0.13f));
            g.fillEllipse (lc.x - gr, lc.y - gr, gr * 2.0f, gr * 2.0f);
        }
        g.setGradientFill (ColourGradient (led.brighter (0.9f), lc.x - 2.0f, lc.y - 3.0f, led, lc.x + lr, lc.y + lr, true));
    }
    else
    {
        g.setGradientFill (ColourGradient (led.darker (2.2f), lc.x, lc.y - lr, led.darker (3.5f), lc.x, lc.y + lr, false));
    }
    g.fillEllipse (lc.x - lr, lc.y - lr, lr * 2.0f, lr * 2.0f);
    g.setColour (Colours::white.withAlpha (on ? 0.7f : 0.15f));
    g.fillEllipse (lc.x - 3.0f, lc.y - 4.5f, 3.5f, 2.5f);
}

//==============================================================================
// Editor
//==============================================================================
AuraBassAudioProcessorEditor::AuraBassAudioProcessorEditor (AuraBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    const std::array<std::pair<const char*, const char*>, 7> layout { {
        { ParamIDs::drive,    "DRIVE" },
        { ParamIDs::blend,    "BLEND" },
        { ParamIDs::low,      "LOW" },
        { ParamIDs::mid,      "MID" },
        { ParamIDs::high,     "HIGH" },
        { ParamIDs::presence, "PRESENCE" },
        { ParamIDs::level,    "LEVEL" } } };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.paramId = layout[i].first;
        k.name = layout[i].second;

        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
        k.slider.setMouseCursor (MouseCursor::UpDownResizeCursor);
        addAndMakeVisible (k.slider);

        k.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, k.paramId, k.slider);

        // Duplo clique volta ao valor padrão
        auto* param = processor.apvts.getParameter (k.paramId);
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        k.slider.onDragStart = [&k] { k.lastTouched = now(); };
        k.slider.onValueChange = [this, &k]
        {
            if (k.slider.isMouseButtonDown() || k.slider.isMouseOver())
            {
                k.lastTouched = now();
                repaint();
            }
        };
    }

    groups[0].title = "OVERDRIVE";
    groups[1].title = "TONE";
    groups[2].title = "OUTPUT";

    addAndMakeVisible (driveSwitch);
    addAndMakeVisible (cabSwitch);
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::driveOn, driveSwitch);
    cabAttachment   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::cab, cabSwitch);

    setSize (980, 470);
    timerCallback();
    startTimerHz (20);
}

AuraBassAudioProcessorEditor::~AuraBassAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

String AuraBassAudioProcessorEditor::formatValue (const String& id, float v)
{
    if (id == ParamIDs::drive) return String (v, 1);
    if (id == ParamIDs::blend) return String (roundToInt (v)) + " %";
    return (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
}

void AuraBassAudioProcessorEditor::timerCallback()
{
    // Knobs de Drive e Blend ficam esmaecidos com o DRIVE desligado
    const bool on = processor.apvts.getRawParameterValue (ParamIDs::driveOn)->load() > 0.5f;
    for (auto& k : knobs)
        if (k.paramId == ParamIDs::drive || k.paramId == ParamIDs::blend)
            k.slider.setAlpha (on ? 1.0f : 0.55f);

    // Redesenha enquanto algum rótulo está mostrando valor (para ele voltar ao nome)
    for (auto& k : knobs)
        if (now() - k.lastTouched < 2.0)
        {
            repaint();
            break;
        }
}

void AuraBassAudioProcessorEditor::buildChassisTexture()
{
    const int scale = 2;
    const int w = jmax (1, chassisBounds.getWidth() * scale), h = jmax (1, chassisBounds.getHeight() * scale);
    chassisTexture = Image (Image::RGB, w, h, true);
    Graphics ig (chassisTexture);

    ColourGradient base (Colour (0xffd5d6d8), 0.0f, 0.0f, Colour (0xffa4a6a9), 0.0f, (float) h, false);
    base.addColour (0.45, Colour (0xffc6c7ca));
    ig.setGradientFill (base);
    ig.fillAll();

    // Escovado horizontal
    Random rnd (7);
    for (int y = 0; y < h; ++y)
    {
        ig.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rnd.nextFloat() * 0.05f));
        ig.fillRect (0, y, w, 1);
    }
    for (int i = 0; i < 900; ++i)
    {
        const float y = rnd.nextFloat() * (float) h;
        const float x = rnd.nextFloat() * (float) w;
        const float len = 80.0f + rnd.nextFloat() * 500.0f;
        ig.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.035f));
        ig.fillRect (x, y, len, 1.0f);
    }

    // Faixa de luz diagonal suave
    ColourGradient band (Colours::transparentWhite, (float) w * 0.2f, 0.0f, Colours::transparentWhite, (float) w * 0.8f, (float) h, false);
    band.addColour (0.5, Colours::white.withAlpha (0.10f));
    ig.setGradientFill (band);
    ig.fillAll();
}

void AuraBassAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (AuraColours::background);

    const auto chassis = chassisBounds.toFloat();
    constexpr float corner = 24.0f;

    for (int i = 0; i < 6; ++i)   // sombra do chassi
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (chassis.expanded ((float) i).translated (0.0f, 2.0f), corner + (float) i);
    }

    {   // alumínio escovado
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (chassis, corner);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImage (chassisTexture, chassis);
    }

    // Chanfro da borda
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.85f), 0.0f, chassis.getY(),
                                       Colours::black.withAlpha (0.35f), 0.0f, chassis.getBottom(), false));
    g.drawRoundedRectangle (chassis.reduced (1.0f), corner, 2.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (chassis, corner, 1.0f);

    const float sInset = 16.0f;
    drawScrew (g, { chassis.getX() + sInset, chassis.getY() + sInset }, 5.5f, 0.4f);
    drawScrew (g, { chassis.getRight() - sInset, chassis.getY() + sInset }, 5.5f, 1.1f);
    drawScrew (g, { chassis.getX() + sInset, chassis.getBottom() - sInset }, 5.5f, 0.9f);
    drawScrew (g, { chassis.getRight() - sInset, chassis.getBottom() - sInset }, 5.5f, 0.2f);

    // Títulos dos grupos:  ──── TONE ────
    for (auto& grp : groups)
    {
        const auto a = grp.area.toFloat();
        const auto font = Font (FontOptions (11.0f, Font::bold).withKerningFactor (0.3f));
        const float textW = GlyphArrangement::getStringWidth (font, grp.title) + 18.0f;
        const float y = a.getY() + 7.0f;
        drawEngraved (g, grp.title, Rectangle<float> (textW, 14.0f).withCentre ({ a.getCentreX(), y }),
                      Justification::centred, 11.0f, true, 0.3f);
        drawEngravedLine (g, a.getX() + 6.0f, a.getCentreX() - textW * 0.5f, y);
        drawEngravedLine (g, a.getCentreX() + textW * 0.5f, a.getRight() - 6.0f, y);
    }

    // Rótulos dos knobs (mostram o valor por 1,5 s depois de mexer)
    const double t = now();
    for (auto& k : knobs)
    {
        const auto sb = k.slider.getBounds().toFloat();
        const auto area = Rectangle<float> (sb.getWidth() + 30.0f, 16.0f)
                              .withCentre ({ sb.getCentreX(), (float) labelY });
        if (t - k.lastTouched < 1.5)
            drawEngraved (g, formatValue (k.paramId, (float) k.slider.getValue()), area, Justification::centred,
                          13.0f, true, 0.05f, AuraColours::valueText);
        else
            drawEngraved (g, k.name, area, Justification::centred, 12.0f);
    }

    // Separador entre knobs e footswitches
    drawEngravedLine (g, chassis.getX() + 40.0f, chassis.getRight() - 40.0f, (float) separatorY);

    // Logotipo central
    auto wm = wordmarkBounds.toFloat();
    drawEngraved (g, "AURA", wm.removeFromTop (wm.getHeight() * 0.62f), Justification::centredBottom, 44.0f, true, 0.5f);
    drawEngraved (g, "BASS  OVERDRIVE", wm.withTrimmedTop (4.0f), Justification::centredTop, 11.0f, false, 0.45f);
}

void AuraBassAudioProcessorEditor::resized()
{
    chassisBounds = getLocalBounds().reduced (8);
    auto inner = chassisBounds.reduced (36, 26);

    // Parte de cima: três grupos de knobs
    auto top = inner.removeFromTop (230);
    const int gap = 28;
    const int wOver = 290, wOut = 150;
    groups[0].area = top.removeFromLeft (wOver);
    groups[2].area = top.removeFromRight (wOut);
    top.removeFromLeft (gap);
    top.removeFromRight (gap);
    groups[1].area = top;

    auto knobArea = [] (Rectangle<int> a) { return a.withTrimmedTop (22).withTrimmedBottom (36); };
    auto place = [] (Slider& s, Point<int> centre, int size) { s.setBounds (Rectangle<int> (size, size).withCentre (centre)); };

    {   // OVERDRIVE: Drive grande + Blend
        auto a = knobArea (groups[0].area);
        place (knobs[0].slider, { a.getX() + a.getWidth() * 34 / 100, a.getCentreY() }, 166);
        place (knobs[1].slider, { a.getX() + a.getWidth() * 82 / 100, a.getCentreY() }, 124);
    }
    {   // TONE: Low, Mid, High, Presence
        auto a = knobArea (groups[1].area);
        const int colW = a.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            place (knobs[(size_t) (2 + i)].slider, { a.getX() + colW * i + colW / 2, a.getCentreY() }, 102);
    }
    {   // OUTPUT: Level grande
        auto a = knobArea (groups[2].area);
        place (knobs[6].slider, a.getCentre(), 150);
    }

    labelY = top.getBottom() - 18;
    separatorY = inner.getY() + 6;
    inner.removeFromTop (14);

    // Parte de baixo: DRIVE | logotipo | CAB
    const int fsW = 130;
    const int fsH = inner.getHeight();
    driveSwitch.setBounds (Rectangle<int> (fsW, fsH).withCentre ({ inner.getX() + inner.getWidth() * 18 / 100, inner.getCentreY() }));
    cabSwitch.setBounds   (Rectangle<int> (fsW, fsH).withCentre ({ inner.getX() + inner.getWidth() * 82 / 100, inner.getCentreY() }));
    wordmarkBounds = inner.withSizeKeepingCentre (360, fsH);

    buildChassisTexture();
}
