#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
PrismScopeEditor::PrismScopeEditor(PrismScopeProcessor& p) : AudioProcessorEditor(&p), processor(p)
{
    setOpaque(true);
    title.setText("P R I S M S C O P E    /    GENERATIVE ENGINE", juce::dontSendNotification);
    title.setColour(juce::Label::textColourId, juce::Colour(0xffe8e5df));
    title.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    addAndMakeVisible(title);
    for (auto* slider : {&sensitivity, &decay, &hue})
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
        addAndMakeVisible(*slider);
    }
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "gain", sensitivity);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "decay", decay);
    hueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, "hue", hue);
    auto attach = [this, &p](juce::Slider& slider, juce::Label& label, const char* id, const char* text) {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffc4bfb7));
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
        extraAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.parameters, id, slider));
    };
    auto caption = [this](juce::Label& label, const char* text) {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffc4bfb7));
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
    // Independent compositing toggles: generators no longer depend on a universal mode.
    for (auto* toggle : {&particlesOn, &ribbonsOn, &ringsOn, &scopeOn, &spectrumOn, &imageOn})
    {
        toggle->setToggleState(false, juce::dontSendNotification);
        toggle->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffe4e0da));
        addAndMakeVisible(*toggle);
    }
    particlesOn.setToggleState(true, juce::dontSendNotification);
    ribbonsOn.setToggleState(true, juce::dontSendNotification);
    imageOn.setToggleState(true, juce::dontSendNotification);
    attach(particleCount, particleCountLabel, "particleCount", "PARTICLE COUNT");
    attach(particleMotion, particleMotionLabel, "particleMotion", "TURBULENCE");
    attach(ribbonDensity, ribbonDensityLabel, "ribbonDensity", "RIBBON DENSITY");
    attach(ringComplexity, ringComplexityLabel, "ringComplexity", "RING DETAIL");
    attach(backgroundLevel, backgroundLabel, "backgroundLevel", "BACKGROUND");
    attach(glitchAmount, glitchLabel, "glitchAmount", "GLITCH");
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
    bounds.setSizeLimits(900, 550, 2560, 1600);
    addAndMakeVisible(corner);
    setResizable(true, true);
    setSize(1360, 820);
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
    float bass = 0.0f, high = 0.0f;
    for (int i = 2; i < 24; ++i) bass += snapshot.spectrum[static_cast<size_t>(i)];
    for (int i = 180; i < 640; ++i) high += snapshot.spectrum[static_cast<size_t>(i)];
    bassEnergy += (juce::jlimit(0.0f, 1.0f, bass * 2.0f) - bassEnergy) * 0.22f;
    highEnergy += (juce::jlimit(0.0f, 1.0f, high * 0.6f) - highEnergy) * 0.18f;
    phase += (0.012f + audioEnvelope * 0.055f);
    repaint();
}
void PrismScopeEditor::resized()
{
    const auto r = getLocalBounds();
    title.setBounds(20, 8, r.getWidth() - 40, 35);
    const int panelX = juce::jmax(610, r.getWidth() - 390);
    const int panelW = r.getWidth() - panelX - 16;
    int y = 52;
    const juce::ToggleButton* unused = nullptr; juce::ignoreUnused(unused);
    for (auto* toggle : {&particlesOn, &ribbonsOn, &ringsOn, &scopeOn, &spectrumOn, &imageOn})
    {
        toggle->setBounds(panelX + 8, y, panelW - 16, 25); y += 28;
    }
    y += 12;
    struct Pair { juce::Slider* slider; juce::Label* label; };
    const Pair controls[] = {
        {&particleCount, &particleCountLabel}, {&particleMotion, &particleMotionLabel},
        {&ribbonDensity, &ribbonDensityLabel}, {&ringComplexity, &ringComplexityLabel},
        {&backgroundLevel, &backgroundLabel}, {&glitchAmount, &glitchLabel},
        {&sensitivity, &gainLabel}, {&decay, &decayLabel}, {&hue, &hueLabel},
        {&imagePulse, &pulseLabel}, {&imageWarp, &warpLabel}, {&imageSpin, &spinLabel},
        {&glow, &glowLabel}, {&crt, &crtLabel}, {&imageOpacity, &opacityLabel},
        {&imageSize, &sizeLabel}, {&spectrumScale, &spectrumLabel}
    };
    for (const auto& c : controls)
    {
        c.label->setJustificationType(juce::Justification::centredLeft);
        c.label->setBounds(panelX + 10, y, 125, 24);
        c.slider->setBounds(panelX + 138, y, panelW - 148, 24);
        y += 29;
    }
    imageButton.setBounds(panelX + 8, y + 8, 115, 30);
    screenshotButton.setBounds(panelX + 133, y + 8, 115, 30);
    mode.setBounds(panelX + 8, y + 45, 160, 26);
    corner.setBounds(r.getWidth() - 18, r.getHeight() - 18, 18, 18);
}
void PrismScopeEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0b0d));
    auto area = getLocalBounds().reduced(18).withTrimmedTop(40);
    area.setRight(juce::jmax(area.getX() + 200, getWidth() - 408));
    const float bg = processor.parameters.getRawParameterValue("backgroundLevel")->load();
    g.setColour(juce::Colour::fromFloatRGBA(bg * 0.32f, bg * 0.32f, bg * 0.32f, 1.0f));
    g.fillRoundedRectangle(area.toFloat(), 9.0f);
    const float h = static_cast<float>(area.getHeight()), w = static_cast<float>(area.getWidth());
    const float sensitivityValue = processor.parameters.getRawParameterValue("gain")->load();
    const int visualMode = static_cast<int>(processor.parameters.getRawParameterValue("mode")->load());
    const auto accent = juce::Colour::fromHSV(processor.parameters.getRawParameterValue("hue")->load() / 359.0f, 0.72f, 0.96f, 1.0f);
    const float glowAmount = processor.parameters.getRawParameterValue("glow")->load();
    const float barScale = processor.parameters.getRawParameterValue("spectrumScale")->load();
    drawGenerators(g, area);
    g.setColour(juce::Colour(0xff1c1c1e));
    g.drawRoundedRectangle(area.toFloat(), 9.0f, 1.0f);
    if (spectrumOn.getToggleState())
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
    if (scopeOn.getToggleState())
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
    if (imageOn.getToggleState() && imported.isValid())
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

void PrismScopeEditor::drawGenerators(juce::Graphics& g, juce::Rectangle<int> area)
{
    const float w = static_cast<float>(area.getWidth());
    const float h = static_cast<float>(area.getHeight());
    const float cx = static_cast<float>(area.getCentreX());
    const float cy = static_cast<float>(area.getCentreY());
    const float hue = processor.parameters.getRawParameterValue("hue")->load() / 359.0f;
    const auto accent = juce::Colour::fromHSV(hue, 0.48f, 0.94f, 1.0f);
    const float density = processor.parameters.getRawParameterValue("particleCount")->load();
    const float motion = processor.parameters.getRawParameterValue("particleMotion")->load();
    const float ribbons = processor.parameters.getRawParameterValue("ribbonDensity")->load();
    const float detail = processor.parameters.getRawParameterValue("ringComplexity")->load();
    const float glitch = processor.parameters.getRawParameterValue("glitchAmount")->load();
    g.saveState();
    g.reduceClipRegion(area);
    if (ribbonsOn.getToggleState())
    {
        const int n = 2 + static_cast<int>(ribbons * 20.0f);
        for (int j = 0; j < n; ++j)
        {
            juce::Path path;
            for (int i = 0; i <= 150; ++i)
            {
                const float t = i / 150.0f;
                const float x = area.getX() + t * w;
                const float wave = std::sin(t * (8.0f + j * 0.43f) + phase * (0.8f + j * 0.035f));
                const float interference = std::sin(t * 28.0f - phase * 0.6f + j * 1.6f);
                const float y = cy + (j - n * 0.5f) * h * 0.026f +
                    (wave * 0.18f + interference * 0.04f) * h * (0.35f + bassEnergy * 1.6f);
                const float displacedX = x + std::sin(t * 72.0f + phase * 9.0f) * glitch * highEnergy * 55.0f;
                if (i == 0) path.startNewSubPath(displacedX, y); else path.lineTo(displacedX, y);
            }
            g.setColour(accent.withAlpha(0.14f + 0.35f * (j % 4) / 4.0f));
            g.strokePath(path, juce::PathStrokeType(0.6f + (j % 3) * 0.5f));
        }
    }
    if (ringsOn.getToggleState())
    {
        const int count = 2 + static_cast<int>(detail * 15.0f);
        for (int j = 0; j < count; ++j)
        {
            juce::Path path;
            for (int i = 0; i <= 240; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * i / 240.0f;
                const float r = juce::jmin(w, h) * (0.11f + j * 0.018f) *
                    (1.0f + 0.2f * std::sin(a * (3 + j % 7) + phase * 2.0f) * (0.3f + bassEnergy));
                const float x = cx + std::cos(a + phase * 0.12f) * r;
                const float y = cy + std::sin(a + phase * 0.12f) * r;
                if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
            }
            g.setColour(accent.withAlpha(0.1f + (j % 5) * 0.075f));
            g.strokePath(path, juce::PathStrokeType(0.7f));
        }
    }
    if (particlesOn.getToggleState())
    {
        const int count = static_cast<int>(density * 750.0f);
        for (int i = 0; i < count; ++i)
        {
            const float seed = static_cast<float>(i);
            const float radius = std::sqrt((i + 0.5f) / juce::jmax(1, count)) * juce::jmin(w, h) * 0.48f;
            const float angle = seed * 2.399963f + phase * (0.18f + motion * 0.7f) * (0.2f + (i % 13) / 13.0f);
            const float turbulence = std::sin(seed * 0.77f + phase * 2.0f) * motion * bassEnergy * 35.0f;
            const float x = cx + std::cos(angle) * (radius + turbulence);
            const float y = cy + std::sin(angle) * (radius + turbulence) * 0.75f;
            g.setColour(accent.withAlpha(0.18f + (i % 7) * 0.09f));
            g.fillEllipse(x, y, 0.6f + (i % 5) * 0.32f, 0.6f + (i % 5) * 0.32f);
        }
    }
    g.restoreState();
}
