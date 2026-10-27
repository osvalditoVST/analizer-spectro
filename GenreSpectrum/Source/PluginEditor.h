#pragma once
#include "PluginProcessor.h"

class SpectrumView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumView (GenreSpectrumProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override { mousePos = e.position; }
    void mouseExit (const juce::MouseEvent&) override { mousePos = { -1.0f, -1.0f }; }
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void rebuildBands();
    void update();
    float refAt (double f, const std::vector<std::pair<float,float>>& c) const;
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    GenreSpectrumProcessor& proc;
    int curDiv = 0;
    double curSr = 0.0;
    float floorDb = -60.0f, slopeDb = 3.0f, refOffset = -12.0f;
    std::vector<double> bLo, bHi, bFc;
    std::vector<float> bar, peak, avg;
    juce::Point<float> mousePos { -1.0f, -1.0f };
};

class GenreSpectrumEditor : public juce::AudioProcessorEditor
{
public:
    explicit GenreSpectrumEditor (GenreSpectrumProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupCombo (juce::ComboBox&, const char* id);

    GenreSpectrumProcessor& proc;
    juce::ComboBox preset, bands, range, slope, speed;
    juce::Label lPreset, lBands, lRange, lSlope, lSpeed;
    juce::ToggleButton hold { "Hold" }, freeze { "Freeze" }, ref { "Ref" };
    SpectrumView view;
    std::vector<std::unique_ptr<CA>> comboAtt;
    std::vector<std::unique_ptr<BA>> buttonAtt;
};
