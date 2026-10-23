#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    using namespace juce;
    namespace L = AuraLayout;

    Image load (const void* data, int size) { return ImageCache::getFromMemory (data, size); }

    Rectangle<float> square (float cx, float cy, float half) { return { cx - half, cy - half, 2 * half, 2 * half }; }
    Rectangle<int> toInt (const L::Rect& r) { return Rectangle<float> (r.x, r.y, r.w, r.h).getSmallestIntegerContainer(); }
}

//==============================================================================
AuraAssets::AuraAssets()
{
    background = load (BinaryData::background_jpg, BinaryData::background_jpgSize);
    pointer    = load (BinaryData::pointer_png, BinaryData::pointer_pngSize);
    ledDriveOn = load (BinaryData::led_drive_on_png, BinaryData::led_drive_on_pngSize);
    ledCabOn   = load (BinaryData::led_cab_on_png, BinaryData::led_cab_on_pngSize);
    cabOff     = load (BinaryData::cab_off_png, BinaryData::cab_off_pngSize);
    valueTypeface = Typeface::createSystemTypefaceFor (BinaryData::Roboto_ttf, (size_t) BinaryData::Roboto_ttfSize);
}

//==============================================================================
PointerKnob::PointerKnob (const AuraAssets& a)
    : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox), assets (a)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setMouseDragSensitivity (220);
}

bool PointerKnob::hitTest (int x, int y)
{
    return centre.getDistanceFrom ({ (float) x, (float) y }) <= L::knobHitRadius;
}

void PointerKnob::paint (Graphics& g)
{
    const float norm = (float) valueToProportionOfLength (getValue());
    const float angle = degreesToRadians (L::knobMinAngleDeg + norm * (L::knobMaxAngleDeg - L::knobMinAngleDeg));
    const auto c = centre;
    const auto& p = assets.pointer;

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImageTransformed (p, AffineTransform::translation (-(float) p.getWidth() * 0.5f, -(float) p.getHeight() * 0.5f)
                                   .scaled ((2.0f * L::pointerHalf) / (float) p.getWidth())
                                   .rotated (angle)
                                   .translated (c));
}

//==============================================================================
AuraBassAudioProcessorEditor::AuraBassAudioProcessorEditor (AuraBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
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
    addAndMakeVisible (cabSpot);
    driveAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::driveOn, driveSpot);
    cabAttachment   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::cab, cabSpot);

    setSize (L::width, L::height);
    timerCallback();
    startTimerHz (20);
}

AuraBassAudioProcessorEditor::~AuraBassAudioProcessorEditor()
{
    stopTimer();
}

String AuraBassAudioProcessorEditor::formatValue (const String& id, float v)
{
    if (id == ParamIDs::drive) return String (v, 2);
    if (id == ParamIDs::blend) return String (v, 1) + "%";
    if (std::abs (v) < 0.05f) v = 0.0f;
    return String (v, 1) + " dB";
}

void AuraBassAudioProcessorEditor::timerCallback()
{
    const bool d = processor.apvts.getRawParameterValue (ParamIDs::driveOn)->load() > 0.5f;
    const bool c = processor.apvts.getRawParameterValue (ParamIDs::cab)->load() > 0.5f;
    if (d != driveOn || c != cabOn)
    {
        driveOn = d; cabOn = c;
        repaint();
    }
}

void AuraBassAudioProcessorEditor::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (assets->background, getLocalBounds().toFloat());

    if (driveOn)
        g.drawImage (assets->ledDriveOn, square (L::driveLedX, L::driveLedY, L::ledHalf));

    if (cabOn)
        g.drawImage (assets->ledCabOn, square (L::cabLedX, L::cabLedY, L::ledHalf));
    else
        g.drawImage (assets->cabOff, square (L::cabToggleX, L::cabToggleY, L::toggleHalf));
}

void AuraBassAudioProcessorEditor::paintOverChildren (Graphics& g)
{
    // Valores ao vivo embaixo de cada knob
    const auto font = Font (FontOptions (assets->valueTypeface).withHeight (L::valueFontSize));
    g.setFont (font);
    g.setColour (Colour (0xffb7bcc4));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto& k = L::knobs[i];
        const auto text = formatValue (k.paramId, (float) knobs[i]->getValue());
        g.drawText (text, Rectangle<float> (110.0f, 20.0f).withCentre ({ k.valueX, L::valueY }), Justification::centred, false);
    }
}

void AuraBassAudioProcessorEditor::resized()
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
