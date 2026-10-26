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
    valueTypeface = Typeface::createSystemTypefaceFor (BinaryData::Roboto_ttf, (size_t) BinaryData::Roboto_ttfSize);
}

//==============================================================================
PointerKnob::PointerKnob (const AuraAssets& a)
    : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox), assets (a)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setMouseDragSensitivity (normalDragPixels);
}

bool PointerKnob::hitTest (int x, int y)
{
    return centre.getDistanceFrom ({ (float) x, (float) y }) <= L::knobHitRadius;
}

void PointerKnob::mouseDown (const MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? fineDragPixels : normalDragPixels);
    Slider::mouseDown (e);
}

void PointerKnob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (e.mods.isShiftDown())
    {
        // Roda do mouse com Shift: um passo mínimo (0,1 dB / 1%) por movimento
        const auto step = getInterval() > 0.0 ? getInterval() : 0.1;
        const float dir = (std::abs (wheel.deltaY) > 1.0e-6f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
        if (std::abs (dir) > 1.0e-6f)
            setValue (getValue() + (dir > 0 ? step : -step), sendNotificationSync);
        return;
    }
    Slider::mouseWheelMove (e, wheel);
}

void PointerKnob::paint (Graphics& g)
{
    const float norm = (float) valueToProportionOfLength (getValue());
    const float angle = degreesToRadians (L::knobMinAngleDeg + norm * (L::knobMaxAngleDeg - L::knobMinAngleDeg));
    const auto& p = assets.pointer;

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (p, AffineTransform::translation (-(float) p.getWidth() * 0.5f, -(float) p.getHeight() * 0.5f)
                                   .scaled ((2.0f * L::pointerHalf) / (float) p.getWidth())
                                   .rotated (angle)
                                   .translated (centre));
}

//==============================================================================
AuraPanel::AuraPanel (AuraBassAudioProcessor& p) : processor (p)
{
    for (const auto& k : L::knobs)
    {
        auto knob = std::make_unique<PointerKnob> (*assets);
        addAndMakeVisible (*knob);
        knobAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, k.paramId, *knob));
        auto* param = processor.apvts.getParameter (k.paramId);
        knob->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        knob->onValueChange = [this] { repaint(); };
        knobs.push_back (std::move (knob));
    }

    addAndMakeVisible (driveSpot);
    driveSpot.onStateChange = [this] { repaint(); };
    addAndMakeVisible (cabSpot);
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::driveOn, driveSpot);
    cabAttachment   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::cab, cabSpot);

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

void AuraPanel::timerCallback()
{
    const bool d = processor.apvts.getRawParameterValue (ParamIDs::driveOn)->load() > 0.5f;
    const bool c = processor.apvts.getRawParameterValue (ParamIDs::cab)->load() > 0.5f;
    const auto ir = processor.getCabIRDisplayName();
    if (d != driveOn || c != cabOn || ir != cabIrName)
    {
        driveOn = d; cabOn = c; cabIrName = ir;
        repaint();
    }
}

Rectangle<float> AuraPanel::cabIrArea() const   { return Rectangle<float> (64.0f, 18.0f).withCentre ({ L::cabToggleX, 241.0f }); }
Rectangle<float> AuraPanel::scaleArea() const   { return Rectangle<float> (40.0f, 306.0f, 62.0f, 16.0f); }

void AuraPanel::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (assets->background, getLocalBounds().toFloat());

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
    // Valores ao vivo embaixo de cada knob
    g.setFont (Font (FontOptions (assets->valueTypeface).withHeight (L::valueFontSize)));
    g.setColour (valueColour);
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = L::knobs[i];
        g.drawText (formatValue (k.paramId, (float) knobs[i]->getValue()),
                    Rectangle<float> (110.0f, 20.0f).withCentre ({ k.valueX, L::valueY }), Justification::centred, false);
    }

    // Seletor da IR do CAB (embaixo da chave)
    {
        auto r = cabIrArea();
        g.setFont (Font (FontOptions (assets->valueTypeface).withHeight (11.5f)));
        g.setColour (cabOn ? valueColour : smallLabelColour);
        auto name = cabIrName.length() > 9 ? cabIrName.substring (0, 8) + "…" : cabIrName;
        g.drawText (name, r.withTrimmedRight (10.0f), Justification::centred, false);
        Path tri;
        const float tx = r.getRight() - 7.0f, ty = r.getCentreY();
        tri.addTriangle (tx - 3.0f, ty - 1.5f, tx + 3.0f, ty - 1.5f, tx, ty + 2.0f);
        g.fillPath (tri);
    }

    // Seletor de tamanho da interface (canto inferior esquerdo)
    g.setFont (Font (FontOptions (assets->valueTypeface).withHeight (10.5f)));
    g.setColour (smallLabelColour);
    g.drawText ("UI " + String (roundToInt (currentScale * 100.0f)) + "%", scaleArea(), Justification::centredLeft, false);
}

void AuraPanel::mouseMove (const MouseEvent& e)
{
    const auto p = e.position;
    setMouseCursor (cabIrArea().contains (p) || scaleArea().contains (p) ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void AuraPanel::mouseDown (const MouseEvent& e)
{
    if (cabIrArea().contains (e.position)) { showCabMenu(); return; }
    if (scaleArea().contains (e.position) || e.mods.isPopupMenu()) { showScaleMenu(); return; }
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

    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cabIrArea().getSmallestIntegerContainer())),
                     [this] (int r)
    {
        if (r >= 1 && r <= 2)
            processor.setCabIR ("builtin:" + String (r - 1));
        else if (r == 100)
        {
            chooser = std::make_unique<FileChooser> ("Escolha uma resposta de impulso (IR)",
                                                     File::getSpecialLocation (File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                    processor.setCabIR ("file:" + f.getFullPathName());
                timerCallback();
            });
        }
        timerCallback();
    });
}

void AuraPanel::showScaleMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Tamanho da interface");
    const float scales[] = { 1.0f, 1.25f, 1.5f };
    for (int i = 0; i < 3; ++i)
        m.addItem (1 + i, String (roundToInt (scales[i] * 100.0f)) + "%", true, std::abs (scales[i] - currentScale) < 0.01f);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaleArea().getSmallestIntegerContainer())),
                     [this, scales] (int r)
    {
        if (r >= 1 && r <= 3 && onScaleChosen)
            onScaleChosen (scales[r - 1]);
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
    setScale (processor.getUiScale());
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
