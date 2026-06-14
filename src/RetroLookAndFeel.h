#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Farbpalette im Amiga-ProTracker-Stil: Stahlgrau-Blau, dunkles Grid, Bernstein-Cursor
namespace rt
{
    const juce::Colour bg        { 0xff11141c };
    const juce::Colour panel     { 0xff3a4250 };
    const juce::Colour steel     { 0xff7e8ca3 };
    const juce::Colour steelHi   { 0xffaab6c8 };
    const juce::Colour text      { 0xffd6dceb };
    const juce::Colour textDim   { 0xff707a8e };
    const juce::Colour rowBeat   { 0xff1a1f2b };
    const juce::Colour centerBar { 0xff2a3450 };
    const juce::Colour cursor    { 0xffe8a33d };
    const juce::Colour playBar   { 0xff3d5a99 };
    const juce::Colour noteCol   { 0xffe4e9f5 };
    const juce::Colour instCol   { 0xffe8c573 };
    const juce::Colour volCol    { 0xff8fb6e8 };
    const juce::Colour fxCol     { 0xff9be0a8 }; // Effekt-Spalte: sanftes Gruen

    // 16 kraeftige Instrument-Farben, gut lesbar auf dunklem Grund.
    // Slot 01 = Bernstein, 02 = Himmelblau, 03 = Gruen, ...
    const juce::Colour instPalette[16] = {
        juce::Colour (0xffe8a33d), juce::Colour (0xff6dc2ff),
        juce::Colour (0xff7ee787), juce::Colour (0xffff7b72),
        juce::Colour (0xffd2a8ff), juce::Colour (0xffffd866),
        juce::Colour (0xff56d4dd), juce::Colour (0xffff9bce),
        juce::Colour (0xffa5d6ff), juce::Colour (0xffffab70),
        juce::Colour (0xffb4f1b4), juce::Colour (0xfff2cc60),
        juce::Colour (0xff91a7ff), juce::Colour (0xffff8f8f),
        juce::Colour (0xff79e0b8), juce::Colour (0xffd8d8d8),
    };

    inline juce::Colour instColour (int idx)
    {
        return (idx >= 0 && idx < 16) ? instPalette[idx] : noteCol;
    }

    inline juce::Font mono (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              height,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }
}

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, rt::bg);
        setColour (juce::TextButton::buttonColourId, rt::steel);
        setColour (juce::TextButton::buttonOnColourId, rt::cursor);
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0e1118));
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff0e1118));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff262c38));
        setColour (juce::ComboBox::textColourId, rt::text);
        setColour (juce::ComboBox::outlineColourId, rt::steel.withAlpha (0.5f));
        setColour (juce::ComboBox::arrowColourId, rt::steelHi);
        setColour (juce::Slider::trackColourId, rt::steel);
        setColour (juce::Slider::backgroundColourId, juce::Colour (0xff262c38));
        setColour (juce::Slider::textBoxTextColourId, rt::text);
        setColour (juce::Slider::textBoxOutlineColourId, rt::steel.withAlpha (0.4f));
        setColour (juce::Label::textColourId, rt::text);
        setColour (juce::PopupMenu::backgroundColourId, rt::panel);
        setColour (juce::PopupMenu::textColourId, rt::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, rt::playBar);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }
};
