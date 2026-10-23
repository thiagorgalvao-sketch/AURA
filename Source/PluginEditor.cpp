#include "PluginEditor.h"

//==============================================================================
GritLookAndFeel::GritLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
}

void GritLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcR = radius - 2.0f;

    // Trilha
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (face.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Valor
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Corpo do knob
    const float bodyR = radius * 0.72f;
    g.setGradientFill (juce::ColourGradient (face.brighter (0.25f), centre.x, centre.y - bodyR,
                                             face.darker (0.4f), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Ponteiro
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyR + 4.0f, 3.0f, bodyR * 0.5f, 1.5f);
    g.setColour (text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void GritLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (4.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? accent : face.brighter (highlighted ? 0.2f : 0.1f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? panel : dim);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
GritDIAudioProcessorEditor::GritDIAudioProcessorEditor (GritDIAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    addKnob (level,    ParamIDs::level,    "LEVEL");
    addKnob (drive,    ParamIDs::drive,    "DRIVE");
    addKnob (low,      ParamIDs::low,      "LOW");
    addKnob (mid,      ParamIDs::mid,      "MID");
    addKnob (high,     ParamIDs::high,     "HIGH");
    addKnob (presence, ParamIDs::presence, "PRESENCE");
    addKnob (blend,    ParamIDs::blend,    "BLEND");

    addAndMakeVisible (cabButton);
    cabAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, ParamIDs::cab, cabButton);

    setSize (820, 250);
}

GritDIAudioProcessorEditor::~GritDIAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void GritDIAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    if (paramId == ParamIDs::blend)      k.slider.setTextValueSuffix (" %");
    else if (paramId != ParamIDs::drive) k.slider.setTextValueSuffix (" dB");
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, GritLookAndFeel::dim);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, k.slider);
}

void GritDIAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (GritLookAndFeel::panel);

    auto header = getLocalBounds().removeFromTop (54).reduced (20, 0);
    g.setColour (GritLookAndFeel::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GRIT DI", header, juce::Justification::centredLeft);

    g.setColour (GritLookAndFeel::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("BASS OVERDRIVE", header, juce::Justification::centredRight);

    g.setColour (GritLookAndFeel::accent);
    g.fillRect (20, 53, getWidth() - 40, 2);
}

void GritDIAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (50);

    Knob* knobs[] = { &level, &drive, &low, &mid, &high, &presence, &blend };
    const int columns = 8;
    const int colW = area.getWidth() / columns;

    for (auto* k : knobs)
    {
        auto col = area.removeFromLeft (colW);
        k->label.setBounds (col.removeFromTop (22));
        k->slider.setBounds (col.reduced (4, 0));
    }

    cabButton.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 80), 40));
}
