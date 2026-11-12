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
    for (auto [spot, id] : { std::pair<HotSpot*, const char*> { &driveSpot, ParamIDs::driveOn }, { &cabSpot, ParamIDs::cab } })
    {
        auto* param = processor.apvts.getParameter (id);
        spot->onContextMenu = [this, param] { if (onParameterContextMenu) onParameterContextMenu (*param); };
    }

    setSize (L::width, L::height);
    timerCallback();
    startTimerHz (20);
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

    if (cabOn)
        g.drawImage (assets->ledCabOn, square (L::cabLedX, L::cabLedY, L::ledHalf));
    else
        g.drawImage (assets->cabOff, square (L::cabToggleX, L::cabToggleY, L::toggleHalf));
}

void AuraPanel::paintOverChildren (Graphics& g)
{
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
    if (cabIrArea().contains (e.position)) { showCabMenu(); return; }
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
