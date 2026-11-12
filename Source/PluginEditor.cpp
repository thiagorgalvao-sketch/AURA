#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    using namespace juce;
    namespace L = AuraLayout;

    Image load (const void* data, int size) { return ImageCache::getFromMemory (data, size); }

    Rectangle<float> square (float cx, float cy, float half) { return { cx - half, cy - half, 2 * half, 2 * half }; }
    Rectangle<int> toInt (const L::Rect& r) { return Rectangle<float> (r.x, r.y, r.w, r.h).getSmallestIntegerContainer(); }

    constexpr int normalDragPixels = 220;
    constexpr int fineDragPixels   = 1100;   // Shift: 5x mais fino

    const Colour valueColour { 0xffb7bcc4 };
    const Colour smallLabelColour { 0xff8e939a };

    const char* knobTitles[] = { "Level", "Drive", "Low", "Mid", "High", "Presence", "Blend" };
}

//==============================================================================
AuraAssets::AuraAssets()
{
    background = load (BinaryData::background_jpg, BinaryData::background_jpgSize);
    pointer    = load (BinaryData::pointer_png, BinaryData::pointer_pngSize);
    ledDriveOn = load (BinaryData::led_drive_on_png, BinaryData::led_drive_on_pngSize);
    ledCabOn   = load (BinaryData::led_cab_on_png, BinaryData::led_cab_on_pngSize);
    cabOff     = load (BinaryData::cab_off_png, BinaryData::cab_off_pngSize);
    footswitchDown = load (BinaryData::footswitch_down_png, BinaryData::footswitch_down_pngSize);
    arcsLit    = load (BinaryData::arcs_lit_png, BinaryData::arcs_lit_pngSize);
    ledTunerOn = load (BinaryData::led_tuner_on_png, BinaryData::led_tuner_on_pngSize);
    tunerDown  = load (BinaryData::tuner_down_png, BinaryData::tuner_down_pngSize);
    titleTypeface = Typeface::createSystemTypefaceFor (BinaryData::RussoOneRegular_ttf, (size_t) BinaryData::RussoOneRegular_ttfSize);
    valueTypeface = Typeface::createSystemTypefaceFor (BinaryData::Roboto_ttf, (size_t) BinaryData::Roboto_ttfSize);
    valueFont = Font (FontOptions (valueTypeface).withHeight (L::valueFontSize));
}

//==============================================================================
PointerKnob::PointerKnob (const AuraAssets& a)
    : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox), assets (a)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setMouseDragSensitivity (normalDragPixels);
    setPopupMenuEnabled (false);
}

bool PointerKnob::hitTest (int x, int y)
{
    return centre.getDistanceFrom ({ (float) x, (float) y }) <= L::knobHitRadius;
}

void PointerKnob::mouseDown (const MouseEvent& e)
{
    popupClick = e.mods.isPopupMenu();
    if (popupClick)
    {
        // Clique direito não mexe no valor: abre o menu do parâmetro na DAW
        if (onContextMenu) onContextMenu();
        return;
    }
    setMouseDragSensitivity (e.mods.isShiftDown() ? fineDragPixels : normalDragPixels);
    Slider::mouseDown (e);
}

void PointerKnob::mouseDrag (const MouseEvent& e)
{
    if (! popupClick) Slider::mouseDrag (e);
}

void PointerKnob::mouseUp (const MouseEvent& e)
{
    if (! popupClick) Slider::mouseUp (e);
    popupClick = false;
}

void PointerKnob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (e.mods.isShiftDown())
    {
        // Roda do mouse com Shift: um passo mínimo (0,1 dB / 1%) por movimento,
        // dentro de um gesto (a DAW grava a automação corretamente)
        const auto step = getInterval() > 0.0 ? getInterval() : 0.1;
        const float dir = (std::abs (wheel.deltaY) > 1.0e-6f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
        if (std::abs (dir) > 1.0e-6f)
        {
            const ScopedDragNotification drag (*this);
            setValue (getValue() + (dir > 0 ? step : -step), sendNotificationSync);
        }
        return;
    }
    Slider::mouseWheelMove (e, wheel);
}

void PointerKnob::paint (Graphics& g)
{
    const float norm = (float) valueToProportionOfLength (getValue());
    const float angle = degreesToRadians (L::knobMinAngleDeg + norm * (L::knobMaxAngleDeg - L::knobMinAngleDeg));
    const auto& p = assets.pointer;
    if (! p.isValid()) return;

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (p, AffineTransform::translation (-(float) p.getWidth() * 0.5f, -(float) p.getHeight() * 0.5f)
                                   .scaled ((2.0f * L::pointerHalf) / (float) p.getWidth())
                                   .rotated (angle)
                                   .translated (centre));
}

//==============================================================================
void HotSpot::mouseDown (const MouseEvent& e)
{
    popupClick = e.mods.isPopupMenu();
    if (popupClick)
    {
        if (onContextMenu) onContextMenu();
        return;
    }
    ToggleButton::mouseDown (e);
}

void HotSpot::mouseUp (const MouseEvent& e)
{
    if (! popupClick) ToggleButton::mouseUp (e);
    popupClick = false;
}

//==============================================================================
void TunerDisplay::restart()
{
    hasNote = false;
    candidate = -1000;
    candidateCount = 0;
    warmup = 6;          // ~200 ms: a janela do detector ainda tem áudio de antes
}

void TunerDisplay::update (const Tuner::Reading& r)
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    if (warmup > 0) { --warmup; return; }

    if (r.valid)
    {
        lastValid = now;
        if (hasNote && r.midiNote == note)
        {
            cents += 0.35f * (r.cents - cents);     // agulha estável, mas rápida
            hz += 0.35f * (r.hz - hz);
            candidateCount = 0;
        }
        else
        {
            // Só troca de nota depois de duas leituras seguidas iguais (evita piscar)
            candidateCount = (r.midiNote == candidate) ? candidateCount + 1 : 1;
            candidate = r.midiNote;
            if (! hasNote || candidateCount >= 2)
            {
                hasNote = true;
                note = r.midiNote; cents = r.cents; hz = r.hz;
            }
        }
    }
    else if (now - lastValid > 0.6)
    {
        hasNote = false;
    }
    repaint();
}

void TunerDisplay::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const Colour green { 0xff3ddc84 }, amber { 0xffffb347 }, dim { 0xff5b626c }, text { 0xffc9ced6 };

    // Visor rebaixado: vidro escuro com borda
    g.setColour (Colour (0xff050608));
    g.fillRoundedRectangle (b, 9.0f);
    auto glass = b.reduced (2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff11151a), 0.0f, glass.getY(), Colour (0xff090b0e), 0.0f, glass.getBottom(), false));
    g.fillRoundedRectangle (glass, 7.5f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.0f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.045f), 0.0f, glass.getY(), Colours::transparentWhite, 0.0f, glass.getCentreY(), false));
    g.fillRoundedRectangle (glass.withTrimmedBottom (glass.getHeight() * 0.5f), 7.5f);

    const auto small = Font (FontOptions (assets.valueTypeface).withHeight (10.5f).withKerningFactor (0.14f));
    const bool inTune = hasNote && std::abs (cents) <= 3.0f;
    const Colour accent = inTune ? green : amber;

    // Cabeçalho
    auto header = glass.reduced (14.0f, 0.0f).removeFromTop (24.0f);
    g.setFont (small);
    g.setColour (dim);
    g.drawText ("TUNER", header, Justification::centredLeft, false);
    g.drawText ("A4 = 440 Hz    MUTE", header, Justification::centredRight, false);

    // Nota
    const float cx = b.getCentreX();
    const auto noteArea = Rectangle<float> (220.0f, 66.0f).withCentre ({ cx, b.getY() + 58.0f });
    if (hasNote)
    {
        const auto name = Tuner::noteName (note);
        const auto big = Font (FontOptions (assets.titleTypeface).withHeight (64.0f));
        const float w = GlyphArrangement::getStringWidth (big, name);
        g.setFont (big);
        g.setColour (accent);
        g.drawText (name, noteArea, Justification::centred, false);
        g.setFont (Font (FontOptions (assets.titleTypeface).withHeight (20.0f)));
        g.setColour (accent.withAlpha (0.75f));
        g.drawText (String (Tuner::noteOctave (note)), Rectangle<float> (cx + w * 0.5f + 4.0f, noteArea.getBottom() - 34.0f, 30.0f, 22.0f),
                    Justification::centredLeft, false);

        // Setas: a da esquerda acende quando a nota está baixa, a da direita quando está alta
        auto arrow = [&] (bool right, bool lit)
        {
            const float ax = cx + (right ? 1.0f : -1.0f) * 104.0f, ay = noteArea.getCentreY() + 2.0f, s = right ? 1.0f : -1.0f;
            Path p;
            p.addTriangle (ax + 9.0f * s, ay - 11.0f, ax + 9.0f * s, ay + 11.0f, ax - 9.0f * s, ay);   // aponta para o centro
            g.setColour (lit ? amber : dim.withAlpha (0.35f));
            g.fillPath (p);
        };
        arrow (false, ! inTune && cents < 0.0f);     // baixa: aperte a corda
        arrow (true,  ! inTune && cents > 0.0f);     // alta: afrouxe
    }
    else
    {
        g.setFont (Font (FontOptions (assets.titleTypeface).withHeight (48.0f)));
        g.setColour (dim.withAlpha (0.6f));
        g.drawText ("- -", noteArea, Justification::centred, false);
    }

    // Medidor: 41 segmentos de -50 a +50 cents
    constexpr int segments = 41;
    const float segW = 9.0f, gap = 4.0f, total = segments * segW + (segments - 1) * gap;
    const float x0 = cx - total * 0.5f, my = b.getY() + 108.0f;
    const int pos = hasNote ? jlimit (0, segments - 1, roundToInt (cents / 2.5f) + 20) : -1;
    for (int i = 0; i < segments; ++i)
    {
        const bool centre = i == 20;
        const bool major = (i % 4) == 0;
        const float h = centre ? 24.0f : (major ? 16.0f : 12.0f);
        const auto seg = Rectangle<float> (x0 + (float) i * (segW + gap), my - h * 0.5f, segW, h);
        bool lit = false;
        if (hasNote)
            lit = inTune ? std::abs (i - 20) <= 1 : (i >= jmin (20, pos) && i <= jmax (20, pos));
        g.setColour (lit ? accent : (centre ? dim : dim.withAlpha (0.28f)));
        g.fillRoundedRectangle (seg, 2.0f);
    }

    // Rodapé
    auto footer = Rectangle<float> (x0, b.getY() + 126.0f, total, 20.0f);
    g.setFont (small);
    g.setColour (dim);
    g.drawText ("-50", footer, Justification::centredLeft, false);
    g.drawText ("+50", footer, Justification::centredRight, false);
    if (hasNote)
    {
        const int c = roundToInt (cents);
        g.setFont (Font (FontOptions (assets.valueTypeface).withHeight (13.0f)));
        g.setColour (text);
        g.drawText ((c > 0 ? "+" : "") + String (c) + " cents     " + String (hz, 1) + " Hz", footer, Justification::centred, false);
    }
}

//==============================================================================
AuraPanel::AuraPanel (AuraBassAudioProcessor& p) : processor (p)
{
    for (size_t i = 0; i < std::size (L::knobs); ++i)
    {
        const auto& k = L::knobs[i];
        auto knob = std::make_unique<PointerKnob> (*assets);
        knob->setTitle (knobTitles[i]);
        knob->setDescription ("AURA Bass " + String (knobTitles[i]));
        addAndMakeVisible (*knob);
        knobAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, k.paramId, *knob));

        auto* param = processor.apvts.getParameter (k.paramId);
        knob->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        knob->textFromValueFunction = [param] (double v) { return param->getText (param->convertTo0to1 ((float) v), 0); };
        knob->onValueChange = [this, i] { repaintKnobArea (i); };
        knob->onContextMenu = [this, param] { if (onParameterContextMenu) onParameterContextMenu (*param); };
        knobs.push_back (std::move (knob));
    }

    driveSpot.setTitle ("Drive engage");
    cabSpot.setTitle ("Cab");
    addAndMakeVisible (driveSpot);
    addAndMakeVisible (cabSpot);
    driveSpot.onStateChange = [this] { repaint (square (L::driveSwitchX, L::driveSwitchY, L::driveSwitchDownHalf).getSmallestIntegerContainer()); };
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::driveOn, driveSpot);
    cabAttachment   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::cab, cabSpot);
    tunerSpot.setTitle ("Tuner");
    addAndMakeVisible (tunerSpot);
    tunerSpot.onStateChange = [this] { repaint (square (L::tunerSwitchX, L::tunerSwitchY, L::driveSwitchDownHalf).getSmallestIntegerContainer()); };
    tunerAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::tuner, tunerSpot);
    addChildComponent (tunerDisplay);
    tunerDisplay.setBounds (toInt (L::tunerDisplay));

    for (auto [spot, id] : { std::pair<HotSpot*, const char*> { &driveSpot, ParamIDs::driveOn }, { &cabSpot, ParamIDs::cab }, { &tunerSpot, ParamIDs::tuner } })
    {
        auto* param = processor.apvts.getParameter (id);
        spot->onContextMenu = [this, param] { if (onParameterContextMenu) onParameterContextMenu (*param); };
    }

    setSize (L::width, L::height);
    timerCallback();
    startTimerHz (30);
}

AuraPanel::~AuraPanel()
{
    stopTimer();
}

String AuraPanel::formatValue (const String& id, float v)
{
    if (id == ParamIDs::blend) return String (roundToInt (v)) + "%";
    if (std::abs (v) < 0.05f) v = 0.0f;
    return String (v, 1) + " dB";
}

void AuraPanel::repaintKnobArea (size_t index)
{
    // Só o arco e o valor desse knob (o indicador se redesenha sozinho)
    const auto& k = L::knobs[index];
    const float r = L::arcLitRadius + 2.0f;
    repaint (Rectangle<float> (k.cx - r, k.cy - r, 2 * r, (L::valueY + 12.0f) - (k.cy - r)).getSmallestIntegerContainer());
}

void AuraPanel::timerCallback()
{
    const bool d = processor.apvts.getRawParameterValue (ParamIDs::driveOn)->load() > 0.5f;
    const bool c = processor.apvts.getRawParameterValue (ParamIDs::cab)->load() > 0.5f;
    const int irVersion = processor.getCabIRVersion();
    const bool t = processor.apvts.getRawParameterValue (ParamIDs::tuner)->load() > 0.5f;

    if (t != tunerOn)
    {
        tunerOn = t;
        if (t) tunerDisplay.restart();
        tunerDisplay.setVisible (t);
        repaint (square (L::tunerLedX, L::tunerLedY, L::driveLedHalf).getSmallestIntegerContainer());
    }
    if (tunerOn)
        tunerDisplay.update (processor.getTuner().analyse());

    if (d != driveOn)
    {
        driveOn = d;
        repaint (square (L::driveLedX, L::driveLedY, L::driveLedHalf).getSmallestIntegerContainer());
    }
    if (c != cabOn || irVersion != cabIrVersion)
    {
        cabOn = c;
        cabIrVersion = irVersion;
        cabIrName = processor.getCabIRDisplayName();
        repaint (toInt (L::cabHit).getUnion (cabIrArea().getSmallestIntegerContainer()).expanded (8));
    }
}

Rectangle<float> AuraPanel::cabIrArea() const   { return Rectangle<float> (64.0f, 20.0f).withCentre ({ L::cabToggleX, L::valueY }); }

void AuraPanel::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (assets->background, getLocalBounds().toFloat());

    // Arcos acesos até o valor: knobs em dB acendem a partir do centro (0 dB), o Blend a partir do início
    const auto clip = g.getClipBounds().toFloat();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = L::knobs[i];
        const float r = L::arcLitRadius;
        const auto area = Rectangle<float> (k.cx - r, k.cy + L::arcCentreDy - r, 2 * r, 2 * r);
        if (! clip.intersects (area)) continue;

        const float norm = (float) knobs[i]->valueToProportionOfLength (knobs[i]->getValue());
        const float end = degreesToRadians (-L::arcMaxDeg + norm * 2.0f * L::arcMaxDeg);
        const bool bipolar = String (k.paramId) != ParamIDs::blend;
        const float start = bipolar ? 0.0f : degreesToRadians (-L::arcMaxDeg);
        if (std::abs (end - start) < 0.01f) continue;

        Path pie;
        pie.addPieSegment (area, jmin (start, end), jmax (start, end), 0.0f);
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pie);
        g.drawImage (assets->arcsLit, getLocalBounds().toFloat());
    }

    if (driveSpot.isDown())
        g.drawImage (assets->footswitchDown, square (L::driveSwitchX, L::driveSwitchY, L::driveSwitchDownHalf));
    if (driveOn)
        g.drawImage (assets->ledDriveOn, square (L::driveLedX, L::driveLedY, L::driveLedHalf));

    if (tunerSpot.isDown())
        g.drawImage (assets->tunerDown, square (L::tunerSwitchX, L::tunerSwitchY, L::driveSwitchDownHalf));
    if (tunerOn)
        g.drawImage (assets->ledTunerOn, square (L::tunerLedX, L::tunerLedY, L::driveLedHalf));

    if (cabOn)
        g.drawImage (assets->ledCabOn, square (L::cabLedX, L::cabLedY, L::ledHalf));
    else
        g.drawImage (assets->cabOff, square (L::cabToggleX, L::cabToggleY, L::toggleHalf));
}

void AuraPanel::paintOverChildren (Graphics& g)
{
    if (tunerOn) return;   // o visor do afinador cobre os valores

    const auto& font = assets->valueFont;
    g.setFont (font);

    // Valores ao vivo embaixo de cada knob
    g.setColour (valueColour);
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = L::knobs[i];
        g.drawText (formatValue (k.paramId, (float) knobs[i]->getValue()),
                    Rectangle<float> (110.0f, 20.0f).withCentre ({ k.valueX, L::valueY }), Justification::centred, false);
    }

    // Seletor da IR do CAB: na mesma linha e no mesmo tamanho dos valores, com a seta colada
    const auto name = cabIrName.length() > 7 ? cabIrName.substring (0, 6) + String::fromUTF8 ("…") : cabIrName;
    const float tw = GlyphArrangement::getStringWidth (font, name);
    const float total = tw + 4.0f + 7.0f;
    const auto c = cabIrArea().getCentre();
    const float x0 = c.x - total * 0.5f;
    g.setColour (cabOn ? valueColour : smallLabelColour);
    g.drawText (name, Rectangle<float> (x0, c.y - 10.0f, tw + 1.0f, 20.0f), Justification::centredLeft, false);
    Path tri;
    const float tx = x0 + tw + 4.0f + 3.5f, ty = c.y + 0.5f;
    tri.addTriangle (tx - 3.5f, ty - 2.0f, tx + 3.5f, ty - 2.0f, tx, ty + 2.5f);
    g.fillPath (tri);
}

void AuraPanel::mouseMove (const MouseEvent& e)
{
    setMouseCursor (cabIrArea().contains (e.position) ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void AuraPanel::mouseDown (const MouseEvent& e)
{
    if (! tunerOn && cabIrArea().contains (e.position)) { showCabMenu(); return; }
    if (e.mods.isPopupMenu()) { showScaleMenu(); return; }   // clique direito: tamanho da interface
}

void AuraPanel::showCabMenu()
{
    const auto current = processor.getCabIR();
    PopupMenu m;
    m.addSectionHeader ("Resposta de impulso do CAB");
    const auto names = AuraBassAudioProcessor::builtinCabNames();
    for (int i = 0; i < names.size(); ++i)
        m.addItem (1 + i, names[i] + " (embutida)", true, current == "builtin:" + String (i));
    m.addSeparator();
    if (current.startsWith ("file:"))
        m.addItem (99, "Arquivo: " + processor.getCabIRDisplayName(), false, true);
    m.addItem (100, "Carregar IR (.wav / .aiff)...");

    // SafePointer: o menu pode fechar depois que a janela do plugin já foi fechada
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this)
                                         .withTargetScreenArea (localAreaToGlobal (cabIrArea().getSmallestIntegerContainer())),
                     [safe = SafePointer<AuraPanel> (this)] (int r)
    {
        if (safe == nullptr) return;
        if (r >= 1 && r <= 2)
            safe->processor.setCabIR ("builtin:" + String (r - 1));
        else if (r == 100)
            safe->chooseIrFile();
        safe->timerCallback();
    });
}

void AuraPanel::chooseIrFile()
{
    // Começa na pasta da última IR escolhida (ou em Documentos)
    auto startDir = File::getSpecialLocation (File::userDocumentsDirectory);
    const auto current = processor.getCabIR();
    if (current.startsWith ("file:"))
    {
        const File f (current.fromFirstOccurrenceOf ("file:", false, false));
        if (f.getParentDirectory().isDirectory()) startDir = f.getParentDirectory();
    }

    chooser = std::make_unique<FileChooser> ("Escolha uma resposta de impulso (IR)", startDir, "*.wav;*.aif;*.aiff");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe = SafePointer<AuraPanel> (this)] (const FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (f.existsAsFile())
            safe->processor.setCabIR ("file:" + f.getFullPathName());
        safe->timerCallback();
    });
}

void AuraPanel::showScaleMenu()
{
    PopupMenu m;
    m.addSectionHeader ("AURA Bass " + String (JucePlugin_VersionString));
    m.addSeparator();
    m.addSectionHeader ("Tamanho da interface");
    const float scales[] = { 1.0f, 1.25f, 1.5f };
    for (int i = 0; i < 3; ++i)
        m.addItem (1 + i, String (roundToInt (scales[i] * 100.0f)) + "%", true, std::abs (scales[i] - currentScale) < 0.01f);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
                     [safe = SafePointer<AuraPanel> (this), scales] (int r)
    {
        if (safe != nullptr && r >= 1 && r <= 3 && safe->onScaleChosen)
            safe->onScaleChosen (scales[r - 1]);
    });
}

void AuraPanel::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = L::knobs[i];
        const auto b = square (k.cx, k.cy, L::knobHitRadius + 1.0f).getSmallestIntegerContainer();
        knobs[i]->setBounds (b);
        knobs[i]->centre = { k.cx - (float) b.getX(), k.cy - (float) b.getY() };
    }
    driveSpot.setBounds (toInt (L::driveHit));
    tunerSpot.setBounds (toInt (L::tunerHit));
    cabSpot.setBounds (toInt (L::cabHit));
}

//==============================================================================
AuraBassAudioProcessorEditor::AuraBassAudioProcessorEditor (AuraBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.onScaleChosen = [this] (float s) { setScale (s); };
    panel.onParameterContextMenu = [this] (RangedAudioParameter& param) { showHostMenu (param); };
    setScale (processor.getUiScale());
}

void AuraBassAudioProcessorEditor::showHostMenu (RangedAudioParameter& param)
{
    // Menu da DAW para o parâmetro (automação, MIDI learn etc.), quando a DAW oferece
    if (auto* host = getHostContext())
        if (auto menu = host->getContextMenuForParameter (&param))
        {
            menu->showNativeMenu (getMouseXYRelative());
            return;
        }
    // DAW sem menu de parâmetro (ex.: AU no Logic): mostra o menu do próprio plugin
    panel.showScaleMenu();
}

void AuraBassAudioProcessorEditor::setScale (float scale)
{
    scale = jlimit (1.0f, 1.5f, scale);
    processor.setUiScale (scale);
    panel.currentScale = scale;
    panel.setTransform (AffineTransform::scale (scale));
    setSize (roundToInt ((float) L::width * scale), roundToInt ((float) L::height * scale));
    panel.repaint();
}

void AuraBassAudioProcessorEditor::resized()
{
    panel.setBounds (0, 0, L::width, L::height);
}
