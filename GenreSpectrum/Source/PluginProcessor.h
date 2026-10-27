#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class GenreSpectrumProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder;   // 8192
    static constexpr int numBins  = fftSize / 2;

    GenreSpectrumProcessor();

    const juce::String getName() const override { return "Genre Spectrum"; }
    void prepareToPlay (double sr, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>, numBins> magnitude;   // amplitud lineal (1.0 = 0 dBFS senoidal)
    std::atomic<double> sampleRateAtomic { 44100.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushSample (float s);

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenreSpectrumProcessor)
};
