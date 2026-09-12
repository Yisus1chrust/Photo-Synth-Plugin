# Photo Synth (Paint A Synth)

> Turn any image, color palette, and visual texture into a living polyphonic synthesizer sound.

Created by **Modular Sound Technologies**.

---

## 🚀 How to Compile into VST3 & AU on GitHub (Zero-Coding Guide)

This repository includes a fully configured **GitHub Actions CI/CD pipeline** (`.github/workflows/build-plugins.yml`) that automatically compiles:
- **macOS Universal Binary (VST3 & AU/Component)** — Native support for Apple Silicon (M1/M2/M3/M4) & Intel Macs (Logic Pro, Ableton Live, GarageBand, Reaper, Bitwig).
- **Windows x64 (VST3)** — Native 64-bit VST3 for FL Studio, Ableton Live, Cubase, Studio One, Reaper.
- **Linux x64 (VST3 & Standalone)** — For Bitwig, Reaper, Ardour on Linux.

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
2. You will see the **"Build VST3 & AU Plugins"** workflow running on macOS, Windows, and Linux virtual machines in the cloud.
3. Once completed (approx. 3-5 minutes), scroll to the **Artifacts** section at the bottom of the workflow run:
   - `PhotoSynth-macOS-Universal.zip`
   - `PhotoSynth-Windows-x64.zip`
   - `PhotoSynth-Linux-x64.zip`

---

## 🎛️ Plugin Installation Locations

### macOS (AU & VST3)
Extract `PhotoSynth-macOS-Universal.zip` and move the files to:
- **Audio Unit (AU for Logic Pro, GarageBand):**  
  `/Library/Audio/Plug-Ins/Components/Photo Synth.component`
- **VST3 (Ableton Live, Cubase, Studio One):**  
  `/Library/Audio/Plug-Ins/VST3/Photo Synth.vst3`

### Windows (VST3)
Extract `PhotoSynth-Windows-x64.zip` and copy `Photo Synth.vst3` to:
- `C:\Program Files\Common Files\VST3\`

---

## 🎨 1:1 Parameter & Visual Architecture

| Section | Prototype Feature | Native JUCE VST3 / AU Equivalent |
|---|---|---|
| **Timbre DNA** | Rec. 601 Luminance, Saturation, Sobel Edge, Color Entropy | `ImageAnalyzer.cpp` with 64-bit FNV-1a seed & Sobel convolution |
| **Temporal Eras** | 1800s, 1960s, 1970s, 1980s, 2000s | Multi-harmonic oscillator spread & analog ladder saturation |
| **A/B Morph** | Real-time crossfade interpolation between 2 photos | `apvts` parameter `"morph"` smooth parameter blending |
| **Oscilloscope** | Real-time audio waveform visualization | 512-sample lock-free FIFO buffer in `PluginProcessor.cpp` |
| **Controls** | Cutoff, Resonance, LFO Rate/Depth, ADSR, Limiter, Drive | 12 APVTS thread-safe parameters with DAW automation |
| **UI Aesthetics** | Dark graphite `#09090b` chassis with neon LED arcs | `SynthLookAndFeel.h` custom vector rendering |
