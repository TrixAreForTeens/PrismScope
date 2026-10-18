#pragma once
#include <JuceHeader.h>
#include <array>

class PrismScopeProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopeSize = 2048;
    PrismScopeProcessor();
    ~PrismScopeProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PrismScope"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorValueTreeState parameters;
    struct Snapshot { std::array<float, scopeSize> waveform{}; std::array<float, fftSize / 2> spectrum{}; float peak = 0, rms = 0; };
    Snapshot getSnapshot();
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AbstractFifo fifo { scopeSize * 4 };
    std::array<float, scopeSize * 4> ring{};
    std::array<float, scopeSize> waveform{};
    std::array<float, fftSize * 2> fftData{};
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    juce::SpinLock snapshotLock;
    Snapshot snapshot;
    int writeIndex = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismScopeProcessor)
};
