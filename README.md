# PrismScope — native Ableton visualizer (foundation source)

This is **source code for an initial JUCE VST3 prototype**, not a compiled Windows `.vst3` and not the completed product. It implements audio passthrough, a waveform/spectrum visualization, three display modes, visual sensitivity/hue controls, PNG overlay import, screenshot export, state persistence and resizable UI.

## Build on Windows

Requirements: Visual Studio 2022 with Desktop development with C++, CMake 3.22+, Git, and an internet connection for JUCE FetchContent.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PrismScope_VST3
```

The resulting VST3 bundle will be under `build/PrismScope_artefacts/Release/VST3/`. Copy the `.vst3` bundle to `C:\Program Files\Common Files\VST3\`, then rescan plugins in Ableton Live 12. The project has **not** been compiled or host-tested here.

## Current limitations

- Visualization uses JUCE CPU drawing at a 60 Hz timer, not GPU shaders or 120 FPS.
- PNGs are displayed as overlays; mesh warping is not implemented.
- Screenshots are implemented; video/alpha recording is not.
- No fullscreen independent window, video timeline, modulators, compositing graph, CRT shaders or spectrogram yet.
- The audio/UI shared ring buffer is a **prototype** and needs a properly synchronized lock-free SPSC handoff before production use.
- `decay` is persisted but not yet applied to the spectrum.
- The audio processor does not intentionally change the audio signal.

## Roadmap

See `docs/ARCHITECTURE.md`.

## Automated Windows build

Upload this repository to GitHub, open **Actions → Build Windows VST3 → Run workflow**. Once successful, download the `PrismScope-Windows-VST3` artifact and extract its ZIP. Copy `PrismScope.vst3` to `C:\Program Files\Common Files\VST3\` and rescan in Ableton Live. This build workflow has not yet been executed or validated.
