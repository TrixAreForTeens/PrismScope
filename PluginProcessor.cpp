#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout PrismScopeProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain", 1}, "Visual Sensitivity", juce::NormalisableRange<float>(0.1f, 8.0f, 0.01f), 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay", 1}, "Spectrum Decay", juce::NormalisableRange<float>(0.0f, 0.99f, 0.01f), 0.75f));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Visualizer", juce::StringArray{"Spectrum", "Oscilloscope", "Hybrid"}, 2));
    p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"hue", 1}, "Hue", 0, 359, 185));
    return {p.begin(), p.end()};
}
PrismScopeProcessor::PrismScopeProcessor() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)), parameters(*this, nullptr, "PARAMETERS", createLayout()) {}
void PrismScopeProcessor::prepareToPlay(double, int) { writeIndex = 0; fifo.reset(); }
bool PrismScopeProcessor::isBusesLayoutSupported(const BusesLayout& b) const
{
    return b.getMainInputChannelSet() == b.getMainOutputChannelSet() && (b.getMainInputChannelSet() == juce::AudioChannelSet::mono() || b.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}
void PrismScopeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // Deliberately do not modify the buffer: unity-gain audio passthrough.
    const int channels = buffer.getNumChannels();
    if (channels == 0) return;
    const auto* left = buffer.getReadPointer(0);
    const auto* right = buffer.getReadPointer(channels > 1 ? 1 : 0);
    // Real-time thread: bounded writes, no heap allocations, no graphics or file I/O.
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float mono = 0.5f * (left[i] + right[i]);
        ring[static_cast<size_t>(writeIndex)] = mono;
        writeIndex = (writeIndex + 1) % static_cast<int>(ring.size());
    }
    // FFT and UI snapshot calculation are performed by the editor's timer, not here.
}
PrismScopeProcessor::Snapshot PrismScopeProcessor::getSnapshot()
{
    // This first milestone uses a short lock for copying the latest sample history.
    // The real-time-safe ring/snapshot handoff will be replaced with an atomic SPSC queue.
    Snapshot result;
    const int end = writeIndex;
    for (int i = 0; i < scopeSize; ++i)
        result.waveform[static_cast<size_t>(i)] = ring[static_cast<size_t>((end + static_cast<int>(ring.size()) - scopeSize + i) % static_cast<int>(ring.size()))];
    for (int i = 0; i < fftSize; ++i) fftData[static_cast<size_t>(i)] = result.waveform[static_cast<size_t>(i)];
    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    double sum = 0;
    for (const float v : result.waveform) { result.peak = juce::jmax(result.peak, std::abs(v)); sum += static_cast<double>(v) * v; }
    result.rms = static_cast<float>(std::sqrt(sum / scopeSize));
    for (int i = 0; i < fftSize / 2; ++i) result.spectrum[static_cast<size_t>(i)] = fftData[static_cast<size_t>(i)] / (fftSize * 0.5f);
    return result;
}
void PrismScopeProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void PrismScopeProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* PrismScopeProcessor::createEditor() { return new PrismScopeEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PrismScopeProcessor(); }
