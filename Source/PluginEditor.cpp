#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
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
    auto attach = [this, &p](juce::Slider& slider, juce::Label& label, const char* id, const char* text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 16);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb2c7d0));
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
        extraAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, id, slider));
    };
    auto caption = [this](juce::Label& label, const char* text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb2c7d0));
        addAndMakeVisible(label);
    };
    caption(gainLabel, "SENSITIVITY"); caption(decayLabel, "DECAY"); caption(hueLabel, "HUE");
    attach(imagePulse, pulseLabel, "imagePulse", "PULSE");
    attach(imageWarp, warpLabel, "imageWarp", "WARP");
    attach(imageSpin, spinLabel, "imageSpin", "SPIN");
    attach(glow, glowLabel, "glow", "GLOW");
    attach(crt, crtLabel, "crt", "CRT");
    attach(imageOpacity, opacityLabel, "imageOpacity", "OPACITY");
    attach(imageSize, sizeLabel, "imageSize", "IMAGE SIZE");
    attach(spectrumScale, spectrumLabel, "spectrumScale", "BARS");
    mode.addItemList({"Spectrum", "Oscilloscope", "Hybrid", "Radial"}, 1);
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
    setSize(1280, 760);
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
void PrismScopeEditor::timerCallback()
{
    snapshot = processor.getSnapshot();
    const float target = juce::jlimit(0.0f, 1.0f, snapshot.rms * 5.0f);
    audioEnvelope += (target - audioEnvelope) * (target > audioEnvelope ? 0.30f : 0.075f);
    const float decayFactor = processor.parameters.getRawParameterValue("decay")->load();
    const float gain = processor.parameters.getRawParameterValue("gain")->load();
    for (int i = 0; i < 112; ++i)
    {
        const float f = i / 112.0f;
        const int bin = juce::jlimit(1, PrismScopeProcessor::fftSize / 2 - 1, static_cast<int>(std::pow(2.0f, f * 10.0f)));
        const float db = juce::Decibels::gainToDecibels(snapshot.spectrum[static_cast<size_t>(bin)] * gain, -90.0f);
        const float targetBar = juce::jlimit(0.0f, 1.0f, (db + 75.0f) / 75.0f);
        smoothedBars[static_cast<size_t>(i)] = targetBar > smoothedBars[static_cast<size_t>(i)] ? targetBar : juce::jmax(targetBar, smoothedBars[static_cast<size_t>(i)] * (0.65f + decayFactor * 0.349f));
    }
    phase += (0.004f + audioEnvelope * 0.015f);
    repaint();
}
void PrismScopeEditor::resized()
{
    auto r = getLocalBounds();
    title.setBounds(18, 8, r.getWidth() - 36, 35);
    const int y = r.getHeight() - 178;
    mode.setBounds(18, y + 50, 135, 30);
    struct Pair { juce::Slider* knob; juce::Label* label; };
    const Pair controls[] = {
        {&sensitivity, &gainLabel}, {&decay, &decayLabel}, {&hue, &hueLabel},
        {&imagePulse, &pulseLabel}, {&imageWarp, &warpLabel}, {&imageSpin, &spinLabel},
        {&glow, &glowLabel}, {&crt, &crtLabel}, {&imageOpacity, &opacityLabel},
        {&imageSize, &sizeLabel}, {&spectrumScale, &spectrumLabel}
    };
    const int columns = 11;
    const int start = 165;
    const int width = juce::jmax(62, (r.getWidth() - start - 20) / columns);
    for (int i = 0; i < columns; ++i)
    {
        const int x = start + i * width;
        controls[i].label->setBounds(x, y + 6, width, 20);
        controls[i].knob->setBounds(x + 2, y + 27, width - 4, 94);
    }
    imageButton.setBounds(18, y + 120, 115, 28);
    screenshotButton.setBounds(140, y + 120, 115, 28);
    corner.setBounds(r.getWidth() - 18, r.getHeight() - 18, 18, 18);
}
void PrismScopeEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a1015));
    auto area = getLocalBounds().reduced(18).withTrimmedTop(40).withTrimmedBottom(188);
    g.setColour(juce::Colour(0xff13212a));
    g.fillRoundedRectangle(area.toFloat(), 9.0f);
    const float h = static_cast<float>(area.getHeight()), w = static_cast<float>(area.getWidth());
    const float sensitivityValue = processor.parameters.getRawParameterValue("gain")->load();
    const int visualMode = static_cast<int>(processor.parameters.getRawParameterValue("mode")->load());
    const auto accent = juce::Colour::fromHSV(processor.parameters.getRawParameterValue("hue")->load() / 359.0f, 0.72f, 0.96f, 1.0f);
    const float glowAmount = processor.parameters.getRawParameterValue("glow")->load();
    const float barScale = processor.parameters.getRawParameterValue("spectrumScale")->load();
    g.setColour(juce::Colour(0xff22323c));
    for (int i = 1; i < 8; ++i)
    {
        const auto yy = area.getY() + h * i / 8.0f;
        g.drawHorizontalLine(static_cast<int>(yy), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }
    if (visualMode == 0 || visualMode == 2)
    {
        const float bw = w / 112.0f;
        for (int i = 0; i < 112; ++i)
        {
            const float value = juce::jlimit(0.0f, 1.0f, smoothedBars[static_cast<size_t>(i)] * barScale);
            const float x = area.getX() + i * bw;
            if (glowAmount > 0.01f)
            {
                g.setColour(accent.withAlpha(0.13f * glowAmount));
                g.fillRect(x - 2.0f, area.getBottom() - value * h - 3.0f, juce::jmax(1.0f, bw + 2.0f), value * h + 6.0f);
            }
            g.setColour(accent.withAlpha(0.85f));
            g.fillRect(x, area.getBottom() - value * h, juce::jmax(1.0f, bw - 2.0f), value * h);
        }
    }
    if (visualMode == 1 || visualMode == 2 || visualMode == 3)
    {
        juce::Path path;
        const int count = PrismScopeProcessor::scopeSize;
        for (int i = 0; i < count; ++i)
        {
            float x, y;
            if (visualMode == 3)
            {
                const float angle = juce::MathConstants<float>::twoPi * i / static_cast<float>(count - 1);
                const float radius = juce::jmin(w, h) * (0.24f + snapshot.waveform[static_cast<size_t>(i)] * 0.16f * sensitivityValue);
                x = area.getCentreX() + std::cos(angle) * radius;
                y = area.getCentreY() + std::sin(angle) * radius;
            }
            else
            {
                x = area.getX() + w * i / (count - 1);
                y = area.getCentreY() - snapshot.waveform[static_cast<size_t>(i)] * h * 0.38f * sensitivityValue;
            }
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(accent.withAlpha(0.16f * glowAmount));
        g.strokePath(path, juce::PathStrokeType(5.0f));
        g.setColour(accent.brighter(0.3f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    if (imported.isValid())
    {
        const float pulse = processor.parameters.getRawParameterValue("imagePulse")->load();
        const float spin = processor.parameters.getRawParameterValue("imageSpin")->load();
        const float size = processor.parameters.getRawParameterValue("imageSize")->load();
        const float opacity = processor.parameters.getRawParameterValue("imageOpacity")->load();
        const float side = juce::jmin(w, h) * 0.48f * size * (1.0f + audioEnvelope * pulse * 0.42f);
        auto warped = renderWarpedImage();
        if (warped.isValid())
        {
            const float fit = juce::jmin(side / warped.getWidth(), side / warped.getHeight());
            const float angle = std::sin(phase * 0.6f) * spin * audioEnvelope * 0.8f;
            const auto transform = juce::AffineTransform::scale(fit)
                .rotated(angle, warped.getWidth() * fit * 0.5f, warped.getHeight() * fit * 0.5f)
                .translated(area.getCentreX() - warped.getWidth() * fit * 0.5f, area.getCentreY() - warped.getHeight() * fit * 0.5f);
            g.setOpacity(opacity);
            g.drawImageTransformed(warped, transform, true);
            g.setOpacity(1.0f);
        }
    }
    const float crtAmount = processor.parameters.getRawParameterValue("crt")->load();
    if (crtAmount > 0.01f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f * crtAmount));
        for (int y = area.getY(); y < area.getBottom(); y += 4)
            g.fillRect(area.getX(), y, area.getWidth(), 1);
    }
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(12.0f);
    g.drawText("PEAK " + juce::String(juce::Decibels::gainToDecibels(snapshot.peak, -100.0f), 1) + " dB  |  RMS " + juce::String(juce::Decibels::gainToDecibels(snapshot.rms, -100.0f), 1) + " dB", area.removeFromTop(20), juce::Justification::centredRight);
}

juce::Image PrismScopeEditor::renderWarpedImage() const
{
    if (!imported.isValid()) return {};
    const float warp = processor.parameters.getRawParameterValue("imageWarp")->load() * audioEnvelope;
    if (warp < 0.002f) return imported;
    // CPU prototype: horizontal scanline displacement, retaining PNG alpha.
    // Downscale oversized imports to bound per-frame work on the UI thread.
    const int iw = juce::jmin(imported.getWidth(), 512);
    const int ih = juce::jmin(imported.getHeight(), 512);
    const juce::Image source = imported.rescaled(iw, ih);
    juce::Image result(juce::Image::ARGB, iw, ih, true);
    juce::Graphics rg(result);
    const float shift = warp * iw * 0.14f;
    for (int y = 0; y < ih; y += 2)
    {
        const int strip = juce::jmin(2, ih - y);
        const float dx = std::sin(y * 0.045f + phase * 3.0f) * shift;
        rg.drawImage(source, static_cast<int>(std::round(dx)), y, iw, strip, 0, y, iw, strip);
    }
    return result;
}
