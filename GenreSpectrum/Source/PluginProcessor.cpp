#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

GenreSpectrumProcessor::GenreSpectrumProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& m : magnitude) m.store (0.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout GenreSpectrumProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray names;
    for (auto& p : getPresets()) names.add (p.name);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "preset", 1 }, "Preset", names, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bands", 1 }, "Bandas",
           StringArray { "1/3 oct (30)", "1/6 oct (60)" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "range", 1 }, "Rango dinamico",
           StringArray { "0 a -40 dB", "0 a -50 dB", "0 a -60 dB", "0 a -80 dB", "0 a -100 dB" }, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "slope", 1 }, "Pendiente",
           StringArray { "0 dB/oct", "3 dB/oct", "4.5 dB/oct" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "speed", 1 }, "Velocidad",
           StringArray { "Rapida", "Media", "Lenta" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "hold", 1 },   "Hold", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "ref", 1 },    "Referencia", true));
    return l;
}

void GenreSpectrumProcessor::prepareToPlay (double sr, int)
{
    sampleRateAtomic.store (sr);
    fifoPos = 0;
    fifo.fill (0.0f);
}

bool GenreSpectrumProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void GenreSpectrumProcessor::pushSample (float s)
{
    fifo[(size_t) fifoPos++] = s;
    if (fifoPos < fftSize) return;

    std::copy (fifo.begin(), fifo.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float norm = 4.0f / (float) fftSize;   // Hann: pico de senoidal = A*N/4
    for (int i = 0; i < numBins; ++i)
        magnitude[(size_t) i].store (fftData[(size_t) i] * norm);

    // solape 50 %
    std::copy (fifo.begin() + fftSize / 2, fifo.end(), fifo.begin());
    fifoPos = fftSize / 2;
}

void GenreSpectrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    if (ch == 0) return;

    // El audio pasa intacto; solo se lee para analizar
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < ch; ++c) s += buffer.getReadPointer (c)[i];
        pushSample (s / (float) ch);
    }
}

juce::AudioProcessorEditor* GenreSpectrumProcessor::createEditor() { return new GenreSpectrumEditor (*this); }

void GenreSpectrumProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void GenreSpectrumProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GenreSpectrumProcessor(); }
