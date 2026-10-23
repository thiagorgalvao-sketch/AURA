#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    using namespace juce;
    namespace L = AuraLayout;

    double now() { return Time::getMillisecondCounterHiRes() * 0.001; }

    Image load (const void* data, int size)
    {
        return ImageCache::getFromMemory (data, size);
    }

    // Junta a sequência de quadros (JPEG, sem alfa) com a silhueta do knob (PNG)
    Image composeStrip (const Image& rgb, const Image& mask)
    {
        const int w = rgb.getWidth(), fh = mask.getHeight(), total = rgb.getHeight();
        Image out (Image::ARGB, w, total, true);
        const Image::BitmapData src (rgb, Image::BitmapData::readOnly);
        const Image::BitmapData msk (mask, Image::BitmapData::readOnly);
        Image::BitmapData dst (out, Image::BitmapData::writeOnly);

        for (int y = 0; y < total; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto a = msk.getPixelColour (jmin (x, msk.width - 1), y % fh).getAlpha();
                dst.setPixelColour (x, y, src.getPixelColour (x, y).withAlpha (a));
            }
        return out;
    }

    Rectangle<int> toInt (float x, float y, float w, float h)
    {
        return Rectangle<float> (x, y, w, h).getSmallestIntegerContainer();
    }
}

//==============================================================================
AuraAssets::AuraAssets()
{
    background = load (BinaryData::background_jpg, BinaryData::background_jpgSize);

    const void* strips[3] = { BinaryData::knob_0_jpg, BinaryData::knob_1_jpg, BinaryData::knob_2_jpg };
    const int stripSizes[3] = { BinaryData::knob_0_jpgSize, BinaryData::knob_1_jpgSize, BinaryData::knob_2_jpgSize };
    const void* masks[3] = { BinaryData::knob_0_mask_png, BinaryData::knob_1_mask_png, BinaryData::knob_2_mask_png };
    const int maskSizes[3] = { BinaryData::knob_0_mask_pngSize, BinaryData::knob_1_mask_pngSize, BinaryData::knob_2_mask_pngSize };

    for (int i = 0; i < 3; ++i)
    {
        const auto mask = load (masks[i], maskSizes[i]);
        knobStrips[(size_t) i] = composeStrip (load (strips[i], stripSizes[i]), mask);
        knobFrameHeight[(size_t) i] = mask.getHeight();
    }

    switchDown[0] = load (BinaryData::switch_0_down_jpg, BinaryData::switch_0_down_jpgSize);
    switchDown[1] = load (BinaryData::switch_1_down_jpg, BinaryData::switch_1_down_jpgSize);
    ledOn[0] = load (BinaryData::led_0_on_jpg, BinaryData::led_0_on_jpgSize);
    ledOn[1] = load (BinaryData::led_1_on_jpg, BinaryData::led_1_on_jpgSize);
}

//==============================================================================
FilmstripKnob::FilmstripKnob (const AuraAssets& a, int knobClass)
    : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox), assets (a), cls (knobClass)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
}

void FilmstripKnob::paint (Graphics& g)
{
    const auto& strip = assets.knobStrips[(size_t) cls];
    const int fh = assets.knobFrameHeight[(size_t) cls];
    if (! strip.isValid() || fh <= 0) return;

    const double norm = valueToProportionOfLength (getValue());
    const int frame = jlimit (0, L::knobFrames - 1, (int) std::lround (norm * (L::knobFrames - 1)));

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, frame * fh, strip.getWidth(), fh);
}

//==============================================================================
AuraBassAudioProcessorEditor::AuraBassAudioProcessorEditor (AuraBassAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    for (const auto& k : L::knobs)
    {
        auto knob = std::make_unique<FilmstripKnob> (*assets, k.cls);
        auto* kp = knob.get();
        addAndMakeVisible (*knob);

        knobAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, k.paramId, *knob));

        auto* param = processor.apvts.getParameter (k.paramId);
        knob->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        knob->onDragStart = [this, kp] { kp->lastTouched = now(); repaint(); };
        knob->onValueChange = [this, kp]
        {
            if (kp->isMouseButtonDown() || kp->isMouseOver())
            {
                kp->lastTouched = now();
                repaint();
            }
        };
        knobs.push_back (std::move (knob));
    }

    for (size_t i = 0; i < switches.size(); ++i)
    {
        addAndMakeVisible (switches[i]);
        switches[i].onStateChange = [this] { repaint(); };
        switchAttachments.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, L::switches[i].paramId, switches[i]));
    }

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
    if (id == "drive") return String (v, 1);
    if (id == "blend") return String (roundToInt (v)) + " %";
    return (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
}

void AuraBassAudioProcessorEditor::timerCallback()
{
    bool changed = false;
    for (size_t i = 0; i < 2; ++i)
    {
        const bool on = processor.apvts.getRawParameterValue (L::switches[i].paramId)->load() > 0.5f;
        if (on != ledState[i]) { ledState[i] = on; changed = true; }
    }
    for (auto& k : knobs)
        if (now() - k->lastTouched < 2.0) { changed = true; break; }
    if (changed) repaint();
}

void AuraBassAudioProcessorEditor::paint (Graphics& g)
{
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (assets->background, getLocalBounds().toFloat());

    for (size_t i = 0; i < 2; ++i)
    {
        const auto& sw = L::switches[i];
        if (switches[i].isDown())
        {
            const float h = L::switchCropHalf;
            g.drawImage (assets->switchDown[i], Rectangle<float> (sw.cx - h, sw.cy - h, 2 * h, 2 * h));
        }
        if (ledState[i])
        {
            const float h = L::ledCropHalf;
            g.drawImage (assets->ledOn[i], Rectangle<float> (sw.ledX - h, sw.ledY - h, 2 * h, 2 * h));
        }
    }
}

void AuraBassAudioProcessorEditor::paintOverChildren (Graphics& g)
{
    // Valor do knob sobre o rótulo, logo depois de mexer
    const double t = now();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const double age = t - knobs[i]->lastTouched;
        if (age >= 1.6) continue;
        const float alpha = (float) jlimit (0.0, 1.0, (1.6 - age) / 0.3);
        const auto& k = L::knobs[i];
        const auto text = formatValue (k.paramId, (float) knobs[i]->getValue());
        const auto font = Font (FontOptions (12.5f, Font::bold));
        const float w = GlyphArrangement::getStringWidth (font, text) + 18.0f;
        const auto pill = Rectangle<float> (w, 20.0f).withCentre ({ k.cx, L::knobLabelY });
        g.setColour (Colour (0xff1c1d20).withAlpha (0.88f * alpha));
        g.fillRoundedRectangle (pill, 10.0f);
        g.setColour (Colours::white.withAlpha (alpha));
        g.setFont (font);
        g.drawText (text, pill, Justification::centred);
    }
}

void AuraBassAudioProcessorEditor::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto r = L::knobFrameRect (L::knobs[i]);
        knobs[i]->setBounds (toInt (r.x, r.y, r.w, r.h));
    }

    for (size_t i = 0; i < 2; ++i)
    {
        const auto& sw = L::switches[i];
        const float R = L::switchRadius;
        switches[i].setBounds (toInt (sw.cx - R, sw.cy - R - 8.0f, 2 * R, 2 * R + 4.0f));
    }
}
