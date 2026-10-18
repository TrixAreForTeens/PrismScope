#include "PluginEditor.h"
#include <cmath>
PrismScopeEditor::PrismScopeEditor(PrismScopeProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    setOpaque(true);
    title.setText("PRISMSCOPE   /   AUDIO REACTIVE", juce::dontSendNotification);
    title.setColour(juce::Label::textColourId, juce::Colour(0xffd9e9ec));
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    addAndMakeVisible(title);
    for (auto* slider : {&sensitivity, &decay, &hue})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(*slider);
    }
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "gain", sensitivity);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "decay", decay);
    hueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "hue", hue);
    mode.addItemList({"Spectrum", "Oscilloscope", "Hybrid"}, 1);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.parameters, "mode", mode);
    addAndMakeVisible(mode);
    addAndMakeVisible(imageButton); addAndMakeVisible(screenshotButton);
    imageButton.onClick = [this] { chooseImage(); };
    screenshotButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Save visualizer screenshot", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("PrismScope.png"), "*.png");
        juce::Component::SafePointer<PrismScopeEditor> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fc)
            {
                if (safe == nullptr) return;
                auto file = fc.getResult();
                if (file == juce::File{}) return;
                juce::Image output(juce::Image::ARGB, safe->getWidth(), safe->getHeight(), true);
                juce::Graphics g(output); safe->paintEntireComponent(g, true);
                if (auto stream = file.createOutputStream()) { juce::PNGImageFormat png; png.writeImageToStream(output, *stream); }
            });
    };
    bounds.setSizeLimits(640, 400, 2560, 1600);
    addAndMakeVisible(corner);
    setResizable(true, true);
    setSize(1100, 680);
    startTimerHz(60);
}
void PrismScopeEditor::chooseImage()
{
    chooser = std::make_unique<juce::FileChooser>("Select PNG overlay", juce::File{}, "*.png");
    juce::Component::SafePointer<PrismScopeEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto file = fc.getResult();
            if (file.existsAsFile()) { safe->imported = juce::ImageFileFormat::loadFrom(file); safe->repaint(); }
        });
}
void PrismScopeEditor::timerCallback() { snapshot = processor.getSnapshot(); repaint(); }
void PrismScopeEditor::resized()
{
    auto r = getLocalBounds();
    title.setBounds(18, 8, r.getWidth() - 36, 35);
    const int y = r.getHeight() - 100;
    mode.setBounds(20, y + 24, 150, 30);
    sensitivity.setBounds(180, y, 90, 85);
    decay.setBounds(280, y, 90, 85);
    hue.setBounds(380, y, 90, 85);
    imageButton.setBounds(r.getWidth() - 270, y + 24, 115, 30);
    screenshotButton.setBounds(r.getWidth() - 145, y + 24, 115, 30);
    corner.setBounds(r.getWidth() - 18, r.getHeight() - 18, 18, 18);
}
void PrismScopeEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a1015));
    auto area = getLocalBounds().reduced(18).withTrimmedTop(40).withTrimmedBottom(110);
    g.setColour(juce::Colour(0xff13212a)); g.fillRoundedRectangle(area.toFloat(), 9.0f);
    const float h = static_cast<float>(area.getHeight()), w = static_cast<float>(area.getWidth());
    const float sensitivityValue = processor.parameters.getRawParameterValue("gain")->load();
    const int visualMode = static_cast<int>(processor.parameters.getRawParameterValue("mode")->load());
    const auto accent = juce::Colour::fromHSV(processor.parameters.getRawParameterValue("hue")->load() / 359.0f, 0.72f, 0.96f, 1.0f);
    g.setColour(juce::Colour(0xff22323c));
    for (int i = 1; i < 8; ++i) { const auto yy = area.getY() + h * i / 8.0f; g.drawHorizontalLine(static_cast<int>(yy), static_cast<float>(area.getX()), static_cast<float>(area.getRight())); }
    if (visualMode != 1)
    {
        g.setColour(accent.withAlpha(0.8f));
        const int bars = 112;
        for (int i = 0; i < bars; ++i)
        {
            const float fraction = i / static_cast<float>(bars);
            const int bin = juce::jlimit(1, PrismScopeProcessor::fftSize / 2 - 1, static_cast<int>(std::pow(2.0f, fraction * 10.0f)));
            const float db = juce::Decibels::gainToDecibels(snapshot.spectrum[static_cast<size_t>(bin)] * sensitivityValue, -90.0f);
            const float value = juce::jlimit(0.0f, 1.0f, (db + 75.0f) / 75.0f);
            const float bw = w / bars;
            g.fillRect(area.getX() + i * bw, area.getBottom() - value * h, juce::jmax(1.0f, bw - 2.0f), value * h);
        }
    }
    if (visualMode != 0)
    {
        juce::Path path;
        for (int i = 0; i < PrismScopeProcessor::scopeSize; ++i)
        {
            const float x = area.getX() + w * i / (PrismScopeProcessor::scopeSize - 1);
            const float y = area.getCentreY() - snapshot.waveform[static_cast<size_t>(i)] * h * 0.38f * sensitivityValue;
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(accent.brighter(0.3f)); g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    if (imported.isValid())
    {
        const int side = juce::jmin(area.getWidth(), area.getHeight()) / 2;
        const auto overlay = juce::Rectangle<int>(area.getCentreX() - side / 2, area.getCentreY() - side / 2, side, side);
        g.setOpacity(0.75f); g.drawImageWithin(imported, overlay.getX(), overlay.getY(), overlay.getWidth(), overlay.getHeight(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        g.setOpacity(1.0f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(12.0f);
    g.drawText("PEAK " + juce::String(juce::Decibels::gainToDecibels(snapshot.peak, -100.0f), 1) + " dB  |  RMS " + juce::String(juce::Decibels::gainToDecibels(snapshot.rms, -100.0f), 1) + " dB", area.removeFromTop(20), juce::Justification::centredRight);
}
