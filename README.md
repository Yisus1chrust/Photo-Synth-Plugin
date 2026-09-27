# Photo Synth (Paint A Synth) — Hybrid Audio Plugin

> Turn any image, color palette, and visual texture into a living polyphonic synthesizer sound.

Created by **Modular Sound Technologies**.

---

## ⚡ EVERSYNTH Hybrid Architecture (100% 1:1 Replication)

Photo Synth uses the modern **EVERSYNTH Hybrid Plugin paradigm**:
- **Aesthetic (100% 1:1 Web Interface)**: The exact same responsive, sleek dark cyberpunk interface (HTML5 Canvas oscilloscope, SVG radial timbre gauges, interactive image dropzone with color pipettes, A/B morph crossfader, 6-module studio effects rack, and the custom Modular Sound Technologies logo) renders directly inside your DAW window.
- **Internals (High-Performance C++ DSP Engine)**: Audio synthesis, polyphony allocation, analog 24dB ladder filtering, 2-op FM bells, stereo modulation effects, and lock-free 60 FPS oscilloscope FIFO buffers are calculated in real time in native C++ using JUCE 8.
- **100% Offline & Real-Time**: All web assets are bundled inside the plugin binary and local resource provider (`juce::WebBrowserComponent::Options::withResourceProvider`). It requires **zero internet connection** or local web servers.
- **Bi-Directional Host Sync**: Typing a custom BPM or automating parameters from Ableton/Logic/FL Studio updates both the C++ DSP engine and the visual UI in real time.

---

## 🚀 How to Compile into VST3 & AU on GitHub (Zero-Coding Guide)

This repository includes a fully configured **GitHub Actions CI/CD pipeline** (`.github/workflows/build-plugins.yml`) that automatically:
1. Builds the web frontend into a compact offline bundle (`npm run build`).
2. Configures JUCE 8 CMake with WebView enabled (`JUCE_WEB_BROWSER=1`).
3. Compiles native, universal plugins:
   - **macOS Universal Binary (VST3 & AU/Component)** — Native Apple Silicon (M1/M2/M3/M4) & Intel Macs (Logic Pro, Ableton Live, GarageBand, Reaper, Bitwig).
   - **Windows x64 (VST3 & Standalone)** — FL Studio, Ableton Live, Cubase, Studio One, Reaper.
   - **Linux x64 (VST3 & Standalone)** — Bitwig, Reaper, Ardour on Linux.

### Step 1: Create a GitHub Repository
1. Go to [github.com/new](https://github.com/new) and create a new repository (e.g. `photo-synth`).

### Step 2: Push your code to GitHub
Run these commands in your project folder:
```bash
git remote add origin https://github.com/YOUR_USERNAME/photo-synth.git
git branch -M main
git push -u origin main
```

### Step 3: Cloud Compilation
1. Click the **"Actions"** tab at the top of your GitHub repository.
2. You will see the **"Build VST3 & AU Plugins (Eversynth Hybrid Architecture)"** workflow running on macOS, Windows, and Linux virtual machines in the cloud.
3. Once completed (approx. 3-5 minutes), scroll to the **Artifacts** section at the bottom of the workflow run:
   - `PhotoSynth-macOS-Universal.zip` (Contains AU `.component` and `.vst3` with embedded offline UI)
   - `PhotoSynth-Windows-x64.zip` (Contains 64-bit VST3 and Standalone executable)
   - `PhotoSynth-Linux-x64.zip` (Contains Linux VST3 and Standalone)

---

## 🎛️ Plugin Installation Locations

### macOS (AU & VST3)
Extract `PhotoSynth-macOS-Universal.zip` and move the files to:
- **Audio Unit (AU for Logic Pro, GarageBand):**  
  `/Library/Audio/Plug-Ins/Components/Photo Synth.component`
- **VST3 (Ableton Live, Cubase, Studio One):**  
  `/Library/Audio/Plug-Ins/VST3/Photo Synth.vst3`

### Windows (VST3)
Extract `PhotoSynth-Windows-x64.zip` and copy `Photo Synth.vst3` (along with its `ui` folder) to:
- `C:\Program Files\Common Files\VST3\`

---

## 🎨 1:1 Parameter & Visual Architecture

| Layer | Frontend Aesthetic | Native C++ DSP Engine |
|---|---|---|
| **Branding** | Modular Sound Technologies custom logo | Embedded `LogoBinaryData` |
| **Tempo** | Typed custom BPM (20–300) with single-click edit | Bi-directional host DAW transport sync |
| **Timbre DNA** | Rec. 601 Luminance, Saturation, Sobel Edge, Color Entropy | `ImageAnalyzer.cpp` with 64-bit FNV-1a seed & Sobel convolution |
| **Temporal Eras** | 1800s, 1960s, 1970s, 1980s, 2000s | Multi-harmonic oscillator spread & analog ladder saturation |
| **A/B Morph** | Real-time crossfade interpolation between 2 photos | `apvts` parameter `"morph"` smooth parameter blending |
| **Oscilloscope** | Real-time audio waveform visualization (Canvas) | 512-sample lock-free FIFO buffer in `PluginProcessor.cpp` |
| **Controls** | Cutoff, Resonance, LFO Rate/Depth, ADSR, Limiter, Drive | 12 APVTS thread-safe parameters with DAW automation |
| **Effects Rack** | Delay, Reverb, Chorus, Phaser, Flanger, Distortion | Real-time C++ `EffectsChain.cpp` |
