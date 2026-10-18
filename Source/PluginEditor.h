#pragma once
#include "PluginProcessor.h"
class PrismScopeEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismScopeEditor(PrismScopeProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void chooseImage();
    PrismScopeProcessor& processor;
    PrismScopeProcessor::Snapshot snapshot;
    juce::Slider sensitivity, decay, hue;
    juce::ComboBox mode;
    juce::TextButton imageButton {"Import PNG"}, screenshotButton {"Save PNG"};
    juce::Label title;
    juce::Image imported;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment, decayAttachment, hueAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    juce::ComponentBoundsConstrainer bounds;
    juce::ResizableCornerComponent corner {this, &bounds};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismScopeEditor)
};
