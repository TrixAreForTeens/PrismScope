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
    juce::Slider sensitivity, decay, hue, imagePulse, imageWarp, imageSpin, glow, crt, imageOpacity, imageSize, spectrumScale;
    juce::Label gainLabel, decayLabel, hueLabel, pulseLabel, warpLabel, spinLabel, glowLabel, crtLabel, opacityLabel, sizeLabel, spectrumLabel;
    std::array<float, 112> smoothedBars{};
    float audioEnvelope = 0.0f, phase = 0.0f;
    juce::Image renderWarpedImage() const;
    juce::ComboBox mode;
    juce::ToggleButton particlesOn {"PARTICLES"}, ribbonsOn {"RIBBONS"}, ringsOn {"RINGS"}, scopeOn {"SCOPE"}, spectrumOn {"SPECTRUM"}, imageOn {"IMAGE"};
    juce::Slider particleCount, particleMotion, ribbonDensity, ringComplexity, backgroundLevel, glitchAmount;
    juce::Label particleCountLabel, particleMotionLabel, ribbonDensityLabel, ringComplexityLabel, backgroundLabel, glitchLabel;
    juce::Viewport controlViewport;
    juce::Component controlPanel;
    float bassEnergy = 0.0f, highEnergy = 0.0f;
    void drawGenerators(juce::Graphics&, juce::Rectangle<int>);

    juce::TextButton imageButton {"Import PNG"}, screenshotButton {"Save PNG"};
    juce::Label title;
    juce::Image imported;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment, decayAttachment, hueAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> extraAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    juce::ComponentBoundsConstrainer bounds;
    juce::ResizableCornerComponent corner {this, &bounds};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismScopeEditor)
};
