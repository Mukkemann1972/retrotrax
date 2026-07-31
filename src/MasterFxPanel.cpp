#include "MasterFxPanel.h"
#include <initializer_list>
#include <utility>

MasterFxPanel::MasterFxPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (titleLabel);

    auto setupHead = [this] (juce::Label& l)
    {
        l.setFont (rt::mono (13.0f, true));
        l.setColour (juce::Label::textColourId, rt::steelHi);
        addAndMakeVisible (l);
    };
    auto setupSlider = [this] (juce::Slider& s, juce::Label& lab, double lo, double hi, double step)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.onValueChange = [this] { writeParams(); };
        addAndMakeVisible (s);
        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };

    setupHead (echoHead);
    setupSlider (echoTimeSlider, echoTimeLabel, 20.0, 1000.0, 1.0);
    setupSlider (echoFbSlider,   echoFbLabel,    0.0,   95.0, 1.0);
    setupSlider (echoMixSlider,  echoMixLabel,   0.0,  100.0, 1.0);
    echoTimeSlider.setTextValueSuffix (" ms");
    echoFbSlider.setTextValueSuffix (" %");
    echoMixSlider.setTextValueSuffix (" %");

    setupHead (revHead);
    setupSlider (revSizeSlider, revSizeLabel, 0.0, 100.0, 1.0);
    setupSlider (revMixSlider,  revMixLabel,  0.0, 100.0, 1.0);
    revSizeSlider.setTextValueSuffix (" %");
    revMixSlider.setTextValueSuffix (" %");

    setupHead (eqHead);
    setupSlider (eqLowSlider,  eqLowLabel,  -12.0, 12.0, 0.5);
    setupSlider (eqMidSlider,  eqMidLabel,  -12.0, 12.0, 0.5);
    setupSlider (eqHighSlider, eqHighLabel, -12.0, 12.0, 0.5);
    eqLowSlider.setTextValueSuffix (" dB");
    eqMidSlider.setTextValueSuffix (" dB");
    eqHighSlider.setTextValueSuffix (" dB");

    setupHead (distHead);
    setupSlider (distDriveSlider, distDriveLabel, 0.0, 100.0, 1.0);
    setupSlider (distMixSlider,   distMixLabel,   0.0, 100.0, 1.0);
    distDriveSlider.setTextValueSuffix (" %");
    distMixSlider.setTextValueSuffix (" %");

    setupHead (compHead);
    setupSlider (compThrSlider,   compThrLabel,   -60.0, 0.0, 1.0);
    setupSlider (compRatioSlider, compRatioLabel,   1.0, 20.0, 0.5);
    setupSlider (compMixSlider,   compMixLabel,     0.0, 100.0, 1.0);
    compThrSlider.setTextValueSuffix (" dB");
    compRatioSlider.setTextValueSuffix (" : 1");
    compMixSlider.setTextValueSuffix (" %");

    setupHead (flangHead);
    setupSlider (flangRateSlider,  flangRateLabel,  0.01, 8.0, 0.01);
    setupSlider (flangDepthSlider, flangDepthLabel, 0.0, 100.0, 1.0);
    setupSlider (flangMixSlider,   flangMixLabel,   0.0, 100.0, 1.0);
    flangRateSlider.setTextValueSuffix (" Hz");
    flangDepthSlider.setTextValueSuffix (" %");
    flangMixSlider.setTextValueSuffix (" %");

    setupHead (phaseHead);
    setupSlider (phaseRateSlider,  phaseRateLabel,  0.01, 8.0, 0.01);
    setupSlider (phaseDepthSlider, phaseDepthLabel, 0.0, 100.0, 1.0);
    setupSlider (phaseMixSlider,   phaseMixLabel,   0.0, 100.0, 1.0);
    phaseRateSlider.setTextValueSuffix (" Hz");
    phaseDepthSlider.setTextValueSuffix (" %");
    phaseMixSlider.setTextValueSuffix (" %");

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    refresh();
    applyLanguage();
}

void MasterFxPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("MASTER-FX (wirkt auf alles)", "MASTER FX (affects everything)"),
                        juce::dontSendNotification);
    echoHead.setText (loc::t ("ECHO (Delay)", "ECHO (delay)"), juce::dontSendNotification);
    echoTimeLabel.setText (loc::t ("ZEIT", "TIME"), juce::dontSendNotification);
    echoFbLabel.setText (loc::t ("RUECKKOPPLUNG", "FEEDBACK"), juce::dontSendNotification);
    echoMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    echoMixSlider.setTooltip (loc::t ("Echo-Anteil - 0 = aus", "Echo amount - 0 = off"));
    revHead.setText (loc::t ("HALL (Reverb)", "REVERB"), juce::dontSendNotification);
    revSizeLabel.setText (loc::t ("RAUM", "ROOM"), juce::dontSendNotification);
    revMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    revMixSlider.setTooltip (loc::t ("Hall-Anteil - 0 = aus", "Reverb amount - 0 = off"));
    eqHead.setText (loc::t ("EQ (3-Band)", "EQ (3-band)"), juce::dontSendNotification);
    eqLowLabel.setText (loc::t ("BASS", "LOW"), juce::dontSendNotification);
    eqMidLabel.setText (loc::t ("MITTEN", "MID"), juce::dontSendNotification);
    eqHighLabel.setText (loc::t ("HOEHEN", "HIGH"), juce::dontSendNotification);
    distHead.setText (loc::t ("ZERRE (Distortion)", "DISTORTION"), juce::dontSendNotification);
    distDriveLabel.setText (loc::t ("STAERKE", "DRIVE"), juce::dontSendNotification);
    distMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    distMixSlider.setTooltip (loc::t ("Zerr-Anteil - 0 = aus", "Distortion amount - 0 = off"));
    distDriveSlider.setTooltip (loc::t ("Wie hart angefahren wird - weiche Saettigung, kein hartes Clipping",
                                        "How hard it is driven - soft saturation, not hard clipping"));

    compHead.setText (loc::t ("KOMPRESSOR", "COMPRESSOR"), juce::dontSendNotification);
    compThrLabel.setText (loc::t ("SCHWELLE", "THRESHOLD"), juce::dontSendNotification);
    compRatioLabel.setText (loc::t ("VERHAELTNIS", "RATIO"), juce::dontSendNotification);
    compMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    compMixSlider.setTooltip (loc::t ("0 = aus. Unter 100% bleibt das trockene Signal daneben stehen (Parallel-Kompression)",
                                      "0 = off. Below 100% the dry signal stays alongside (parallel compression)"));
    compThrSlider.setTooltip (loc::t ("Ab welcher Lautstaerke gebremst wird",
                                      "The level above which it starts holding back"));

    flangHead.setText (loc::t ("FLANGER", "FLANGER"), juce::dontSendNotification);
    flangRateLabel.setText (loc::t ("TEMPO", "RATE"), juce::dontSendNotification);
    flangDepthLabel.setText (loc::t ("TIEFE", "DEPTH"), juce::dontSendNotification);
    flangMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    flangMixSlider.setTooltip (loc::t ("Flanger-Anteil - 0 = aus", "Flanger amount - 0 = off"));

    phaseHead.setText (loc::t ("PHASER", "PHASER"), juce::dontSendNotification);
    phaseRateLabel.setText (loc::t ("TEMPO", "RATE"), juce::dontSendNotification);
    phaseDepthLabel.setText (loc::t ("TIEFE", "DEPTH"), juce::dontSendNotification);
    phaseMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    phaseMixSlider.setTooltip (loc::t ("Phaser-Anteil - 0 = aus", "Phaser amount - 0 = off"));

    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
}

void MasterFxPanel::refresh()
{
    loading = true;
    echoTimeSlider.setValue ((double) proc.echoTimeMs.load(),        juce::dontSendNotification);
    echoFbSlider.setValue   ((double) proc.echoFeedback.load() * 100.0, juce::dontSendNotification);
    echoMixSlider.setValue  ((double) proc.echoMix.load() * 100.0,   juce::dontSendNotification);
    revSizeSlider.setValue  ((double) proc.reverbSize.load() * 100.0, juce::dontSendNotification);
    revMixSlider.setValue   ((double) proc.reverbMix.load() * 100.0, juce::dontSendNotification);
    eqLowSlider.setValue    ((double) proc.eqLow.load(),  juce::dontSendNotification);
    eqMidSlider.setValue    ((double) proc.eqMid.load(),  juce::dontSendNotification);
    eqHighSlider.setValue   ((double) proc.eqHigh.load(), juce::dontSendNotification);
    distDriveSlider.setValue  ((double) proc.distDrive.load() * 100.0, juce::dontSendNotification);
    distMixSlider.setValue    ((double) proc.distMix.load()   * 100.0, juce::dontSendNotification);
    compThrSlider.setValue    ((double) proc.compThreshDb.load(),      juce::dontSendNotification);
    compRatioSlider.setValue  ((double) proc.compRatio.load(),         juce::dontSendNotification);
    compMixSlider.setValue    ((double) proc.compMix.load()   * 100.0, juce::dontSendNotification);
    flangRateSlider.setValue  ((double) proc.flangRateHz.load(),       juce::dontSendNotification);
    flangDepthSlider.setValue ((double) proc.flangDepth.load() * 100.0, juce::dontSendNotification);
    flangMixSlider.setValue   ((double) proc.flangMix.load()  * 100.0, juce::dontSendNotification);
    phaseRateSlider.setValue  ((double) proc.phaseRateHz.load(),       juce::dontSendNotification);
    phaseDepthSlider.setValue ((double) proc.phaseDepth.load() * 100.0, juce::dontSendNotification);
    phaseMixSlider.setValue   ((double) proc.phaseMix.load()  * 100.0, juce::dontSendNotification);
    loading = false;
}

void MasterFxPanel::writeParams()
{
    if (loading)
        return;
    proc.echoTimeMs   = (float) echoTimeSlider.getValue();
    proc.echoFeedback = (float) (echoFbSlider.getValue()  / 100.0);
    proc.echoMix      = (float) (echoMixSlider.getValue() / 100.0);
    proc.reverbSize   = (float) (revSizeSlider.getValue() / 100.0);
    proc.reverbMix    = (float) (revMixSlider.getValue()  / 100.0);
    proc.eqLow        = (float) eqLowSlider.getValue();
    proc.eqMid        = (float) eqMidSlider.getValue();
    proc.eqHigh       = (float) eqHighSlider.getValue();
    proc.distDrive    = (float) (distDriveSlider.getValue()  / 100.0);
    proc.distMix      = (float) (distMixSlider.getValue()    / 100.0);
    proc.compThreshDb = (float) compThrSlider.getValue();
    proc.compRatio    = (float) compRatioSlider.getValue();
    proc.compMix      = (float) (compMixSlider.getValue()    / 100.0);
    proc.flangRateHz  = (float) flangRateSlider.getValue();
    proc.flangDepth   = (float) (flangDepthSlider.getValue() / 100.0);
    proc.flangMix     = (float) (flangMixSlider.getValue()   / 100.0);
    proc.phaseRateHz  = (float) phaseRateSlider.getValue();
    proc.phaseDepth   = (float) (phaseDepthSlider.getValue() / 100.0);
    proc.phaseMix     = (float) (phaseMixSlider.getValue()   / 100.0);
}

bool MasterFxPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void MasterFxPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void MasterFxPanel::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (10);

    auto bottom = getLocalBounds().reduced (16).removeFromBottom (32);
    closeButton.setBounds (bottom.removeFromRight (140));
    area.removeFromBottom (40);   // Platz fuer den Knopf freihalten

    // Sieben Effekte passen nicht mehr untereinander -> zwei Spalten. Links die
    // klangformenden (in Durchlaufreihenfolge), rechts Raum und Ton.
    const int gap = 18;
    auto left  = area.removeFromLeft ((area.getWidth() - gap) / 2);
    area.removeFromLeft (gap);
    auto right = area;

    auto block = [] (juce::Rectangle<int>& col, juce::Label& head,
                     std::initializer_list<std::pair<juce::Label*, juce::Slider*>> rows)
    {
        head.setBounds (col.removeFromTop (22));
        col.removeFromTop (4);
        for (auto& r : rows)
        {
            auto row = col.removeFromTop (26);
            r.first->setBounds  (row.removeFromLeft (130));
            r.second->setBounds (row);
            col.removeFromTop (6);
        }
        col.removeFromTop (10);
    };

    // Linke Spalte: erst geformt, dann bewegt.
    block (left, distHead,  { { &distDriveLabel,  &distDriveSlider  },
                              { &distMixLabel,    &distMixSlider    } });
    block (left, flangHead, { { &flangRateLabel,  &flangRateSlider  },
                              { &flangDepthLabel, &flangDepthSlider },
                              { &flangMixLabel,   &flangMixSlider   } });
    block (left, phaseHead, { { &phaseRateLabel,  &phaseRateSlider  },
                              { &phaseDepthLabel, &phaseDepthSlider },
                              { &phaseMixLabel,   &phaseMixSlider   } });

    // Rechte Spalte: Raum, Ton, und zuletzt der Kompressor.
    block (right, echoHead, { { &echoTimeLabel, &echoTimeSlider },
                              { &echoFbLabel,   &echoFbSlider   },
                              { &echoMixLabel,  &echoMixSlider  } });
    block (right, revHead,  { { &revSizeLabel,  &revSizeSlider  },
                              { &revMixLabel,   &revMixSlider   } });
    block (right, eqHead,   { { &eqLowLabel,    &eqLowSlider    },
                              { &eqMidLabel,    &eqMidSlider    },
                              { &eqHighLabel,   &eqHighSlider   } });
    block (right, compHead, { { &compThrLabel,   &compThrSlider   },
                              { &compRatioLabel, &compRatioSlider },
                              { &compMixLabel,   &compMixSlider   } });
}
