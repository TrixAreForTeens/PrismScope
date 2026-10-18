# Production architecture

## Core constraints
- Windows 11 x64; VST3; Ableton Live 12; insert on track/group/Main to visualize that audio; no separate routing required.
- Never block the audio callback, allocate heap memory there, invoke UI code there, or perform GPU/file operations there.
- Plugin must preserve audio samples exactly unless explicit passthrough processing is enabled.
- Dedicated renderer thread and graphics context with safe host window lifecycle, high DPI scaling and device-loss recovery.
- Decouple host audio sample time, transport position, display time, and encoded video frame timestamps.

## Modules
1. **AudioCapture:** bounded SPSC queue, downmix policies, transport metadata, latency accounting, overflow diagnostics.
2. **Analysis:** FFT with overlap/windowing, peak/RMS/LUFS, spectrogram, multiband envelopes, transient/onset, phase/correlation, vectorscope and Lissajous.
3. **RenderGraph:** Vulkan/Direct3D 11 backend, offscreen RGBA16F textures, node scheduling, caching, blend modes, masks, compositing, color management, alpha premultiplication.
4. **Scene:** layer graph, transforms, procedural primitives, particles, feedback buffers, oscilloscope trails, spectrum bars/radial/line, fluid fields, geometry, text and image layers.
5. **Image:** PNG/JPEG import, alpha-preserving texture upload, mesh UV deformation, displacement map, vertex grid warp, edge handling, texture filtering and export.
6. **Modulation:** sources (FFT bands, RMS, transient, tempo, LFO, MIDI optional), attack/release, mapping curves, threshold, hysteresis, clamps, per-parameter fan-out.
7. **FX:** CRT scanlines/curvature, phosphor persistence, VHS jitter, chromatic aberration, bloom, halftone, feedback, kaleidoscope, pixel sorting, color grade.
8. **Recorder:** GPU readback ring, encoder worker, FFmpeg-based video+audio mux; ProRes 4444 or PNG sequence for true alpha; MP4 H.264 is opaque. Separate composited alpha render from editor chrome.
9. **Editor:** MiniMeters-inspired compact monitor; advanced inspector, layers, graph, preset browser, modulation matrix, performance stats, resizable HiDPI view, fullscreen standalone preview window subject to host restrictions.
10. **Persistence:** versioned JSON scene schema embedded in plugin state, external asset references with portable packaging, autosave and preset migrations.

## Milestones and acceptance tests
- M1: Ableton x64 VST3 scan, mono/stereo tracks, null-test transparent passthrough, transport start/stop, save/reload project, reopen editor, DPI/resizing and host stability.
- M2: GPU renderer, multiple independent visualization layers, image alpha + UV warping, renderer lifecycle stress testing.
- M3: modulation routing and automation, transport sync, deterministic scene playback, frequency-band validation.
- M4: offscreen RGBA recording with alpha, audio synchronization, export codec compatibility, dropped-frame handling, fullscreen independent preview.
- M5: CRT/shader library, extensive presets, profiling and packaging, regression suite and Windows installer.

## Performance targets
120 FPS 1920x1080 preview on suitable GPU, quality scaling, render-thread frame pacing, bounded queue depth, memory budget, encoder backpressure policy. Target is not a performance guarantee.

## Audio routing semantics
A VST3 receives only the audio Ableton supplies at its insertion point. On Main it sees the combined mix; on an individual track it sees that track. Cross-track monitoring without routing is not supported by the VST3 audio-bus model.
