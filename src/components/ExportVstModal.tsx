import React, { useState } from 'react';
import { X, Check, Copy, Terminal, Github, Cpu, Layers, Sparkles, Sliders, Globe } from 'lucide-react';
import { getJuceCodeFiles, JuceSourceFile } from '../audio/JuceCodeTemplates';

interface ExportVstModalProps {
  isOpen: boolean;
  onClose: () => void;
}

export const ExportVstModal: React.FC<ExportVstModalProps> = ({ isOpen, onClose }) => {
  const [activeTab, setActiveTab] = useState<'guide' | 'code' | 'specs'>('guide');
  const [selectedFileIdx, setSelectedFileIdx] = useState<number>(0);
  const [copiedKey, setCopiedKey] = useState<string | null>(null);

  if (!isOpen) return null;

  const files: JuceSourceFile[] = getJuceCodeFiles();
  const currentFile = files[selectedFileIdx] || files[0];

  const handleCopy = (text: string, key: string) => {
    navigator.clipboard.writeText(text);
    setCopiedKey(key);
    setTimeout(() => setCopiedKey(null), 2000);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/85 backdrop-blur-md p-3 sm:p-6 animate-in fade-in duration-200">
      <div className="relative w-full max-w-5xl h-[92vh] max-h-[780px] bg-[#0c0d12] border border-[#27272a] rounded-xl shadow-[0_20px_60px_rgba(0,0,0,0.9)] flex flex-col overflow-hidden text-zinc-200 select-none">
        
        {/* Top Header */}
        <div className="flex items-center justify-between px-5 py-3.5 bg-[#141418] border-b border-[#27272a] shrink-0">
          <div className="flex items-center gap-2.5">
            <div className="p-1.5 rounded-md bg-amber-500/10 border border-amber-500/30 text-amber-400">
              <Cpu className="w-4 h-4" />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <span className="font-black tracking-widest text-sm text-white uppercase">
                  EXPORT HYBRID VST3 & AU (EVERSYNTH ARCHITECTURE)
                </span>
                <span className="text-[10px] font-bold px-2 py-0.5 rounded-full bg-emerald-500/15 border border-emerald-500/30 text-emerald-300">
                  100% 1:1 REPLICATION
                </span>
              </div>
              <p className="text-[10px] text-zinc-400 font-mono">
                Web-based aesthetic + Real-time C++ DSP Engine · 100% Offline · macOS (Logic/Ableton) & Windows (FL Studio/Cubase)
              </p>
            </div>
          </div>

          <div className="flex items-center gap-2">
            {/* Tab Switches */}
            <div className="flex bg-[#18181b] p-1 rounded-lg border border-[#27272a] text-xs font-bold font-mono">
              <button
                onClick={() => setActiveTab('guide')}
                className={`px-3 py-1 rounded transition-all ${
                  activeTab === 'guide'
                    ? 'bg-amber-500 text-black shadow-md'
                    : 'text-zinc-400 hover:text-zinc-200'
                }`}
              >
                1-Click GitHub Guide
              </button>
              <button
                onClick={() => setActiveTab('specs')}
                className={`px-3 py-1 rounded transition-all ${
                  activeTab === 'specs'
                    ? 'bg-amber-500 text-black shadow-md'
                    : 'text-zinc-400 hover:text-zinc-200'
                }`}
              >
                Hybrid EVERSYNTH Architecture
              </button>
              <button
                onClick={() => setActiveTab('code')}
                className={`px-3 py-1 rounded transition-all ${
                  activeTab === 'code'
                    ? 'bg-amber-500 text-black shadow-md'
                    : 'text-zinc-400 hover:text-zinc-200'
                }`}
              >
                C++ & CI/CD Files ({files.length})
              </button>
            </div>

            <button
              onClick={onClose}
              className="p-1.5 rounded-md text-zinc-400 hover:text-white hover:bg-zinc-800 transition-all border border-transparent cursor-pointer"
              title="Close Export Window"
            >
              <X className="w-5 h-5" />
            </button>
          </div>
        </div>

        {/* TAB 1: 1-CLICK GITHUB COMPILATION GUIDE */}
        {activeTab === 'guide' && (
          <div className="p-5 overflow-y-auto flex-1 flex flex-col gap-4 font-sans text-xs">
            {/* Overview Banner */}
            <div className="p-4 rounded-xl bg-gradient-to-r from-amber-950/30 via-zinc-900 to-zinc-900 border border-amber-500/30 flex items-start gap-3.5">
              <div className="p-2.5 rounded-lg bg-amber-500/20 text-amber-400 shrink-0">
                <Sparkles className="w-5 h-5" />
              </div>
              <div className="space-y-1">
                <h4 className="font-extrabold text-sm text-zinc-100">
                  How GitHub Builds Your Hybrid VST3 & AU Plugin (Zero Coding Required)
                </h4>
                <p className="text-zinc-300 leading-relaxed text-xs">
                  We have configured this project using the <strong>EVERSYNTH Hybrid Plugin architecture</strong>.
                  When pushed to GitHub, GitHub Actions will:
                  <br />
                  1. Automatically compile the web frontend into a compact, offline bundle (<code className="text-cyan-300 font-mono">npm run build</code>).
                  <br />
                  2. Embed the bundle into the native JUCE 8 C++ plugin binary (<code className="text-amber-300 font-mono">juce::WebBrowserComponent</code>).
                  <br />
                  3. Produce production-ready <code className="text-cyan-300 font-mono">.vst3</code>, <code className="text-pink-300 font-mono">.component (AU)</code>, and Standalone apps that run 100% offline with zero latency!
                </p>
              </div>
            </div>

            {/* 4 Simple Steps */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-3.5">
              {/* Step 1 */}
              <div className="p-3.5 rounded-lg bg-[#12141c] border border-[#232736] flex flex-col gap-2">
                <div className="flex items-center gap-2">
                  <span className="w-6 h-6 rounded-full bg-amber-500/20 text-amber-300 font-black font-mono flex items-center justify-center text-xs">
                    1
                  </span>
                  <span className="font-bold text-zinc-100 uppercase tracking-wide">
                    Create a free GitHub Repository
                  </span>
                </div>
                <p className="text-zinc-400 text-xs leading-normal">
                  Go to <span className="text-zinc-200 font-semibold">github.com/new</span> and create an empty repository named{' '}
                  <span className="text-cyan-300 font-mono font-semibold">photo-synth</span>. Leave it blank without initializing a README.
                </p>
              </div>

              {/* Step 2 */}
              <div className="p-3.5 rounded-lg bg-[#12141c] border border-[#232736] flex flex-col gap-2">
                <div className="flex items-center gap-2">
                  <span className="w-6 h-6 rounded-full bg-amber-500/20 text-amber-300 font-black font-mono flex items-center justify-center text-xs">
                    2
                  </span>
                  <span className="font-bold text-zinc-100 uppercase tracking-wide">
                    Push this Code to your GitHub Repo
                  </span>
                </div>
                <p className="text-zinc-400 text-xs leading-normal">
                  In your terminal or GitHub Desktop, run these 3 standard commands:
                </p>
                <div className="relative bg-[#090a0f] p-2.5 rounded border border-[#1b1e2a] font-mono text-[11px] text-zinc-300 flex items-center justify-between">
                  <code>
                    git remote add origin https://github.com/YOUR_USERNAME/photo-synth.git<br/>
                    git branch -M main<br/>
                    git push -u origin main
                  </code>
                  <button
                    onClick={() => handleCopy('git remote add origin https://github.com/YOUR_USERNAME/photo-synth.git\ngit branch -M main\ngit push -u origin main', 'git-push')}
                    className="p-1.5 rounded hover:bg-zinc-800 text-zinc-400 hover:text-white transition-all shrink-0 ml-2"
                    title="Copy Git commands"
                  >
                    {copiedKey === 'git-push' ? <Check className="w-4 h-4 text-emerald-400" /> : <Copy className="w-4 h-4" />}
                  </button>
                </div>
              </div>

              {/* Step 3 */}
              <div className="p-3.5 rounded-lg bg-[#12141c] border border-[#232736] flex flex-col gap-2">
                <div className="flex items-center gap-2">
                  <span className="w-6 h-6 rounded-full bg-amber-500/20 text-amber-300 font-black font-mono flex items-center justify-center text-xs">
                    3
                  </span>
                  <span className="font-bold text-zinc-100 uppercase tracking-wide">
                    GitHub Compiles in the Cloud (Automated)
                  </span>
                </div>
                <p className="text-zinc-400 text-xs leading-normal">
                  Click the <span className="text-amber-400 font-bold">"Actions"</span> tab in your repository. You will see{' '}
                  <span className="text-cyan-300 font-mono font-semibold">Build VST3 & AU Plugins</span> compiling on macOS, Windows, and Linux virtual machines simultaneously.
                </p>
              </div>

              {/* Step 4 */}
              <div className="p-3.5 rounded-lg bg-[#12141c] border border-[#232736] flex flex-col gap-2">
                <div className="flex items-center gap-2">
                  <span className="w-6 h-6 rounded-full bg-amber-500/20 text-amber-300 font-black font-mono flex items-center justify-center text-xs">
                    4
                  </span>
                  <span className="font-bold text-zinc-100 uppercase tracking-wide">
                    Download & Open in your DAW!
                  </span>
                </div>
                <p className="text-zinc-400 text-xs leading-normal">
                  Click on the finished build run. Under <span className="text-zinc-200 font-bold">"Artifacts"</span>, download:
                </p>
                <div className="text-[11px] text-zinc-300 font-mono space-y-1 bg-[#090a0f] p-2 rounded border border-[#1b1e2a]">
                  <div>• <span className="text-cyan-400 font-bold">PhotoSynth-macOS-Universal.zip</span> (AU for Logic & VST3 for Ableton)</div>
                  <div>• <span className="text-amber-400 font-bold">PhotoSynth-Windows-x64.zip</span> (VST3 for FL Studio, Cubase, etc.)</div>
                </div>
              </div>
            </div>

            {/* Folder Locations Note */}
            <div className="p-3.5 rounded-lg bg-[#11131a] border border-[#202433] flex flex-col gap-1.5 font-mono text-[11px]">
              <span className="text-amber-300 font-bold flex items-center gap-1.5">
                <Terminal className="w-3.5 h-3.5" />
                Plugin Installation Directories:
              </span>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-2 text-zinc-400">
                <div>
                  <span className="text-zinc-200 font-bold">Mac AU (Logic Pro / GarageBand):</span><br/>
                  <code className="text-zinc-300">/Library/Audio/Plug-Ins/Components/</code>
                </div>
                <div>
                  <span className="text-zinc-200 font-bold">Mac VST3 (Ableton Live / Reaper):</span><br/>
                  <code className="text-zinc-300">/Library/Audio/Plug-Ins/VST3/</code>
                </div>
                <div>
                  <span className="text-zinc-200 font-bold">Windows VST3 (FL Studio / Cubase):</span><br/>
                  <code className="text-zinc-300">C:\Program Files\Common Files\VST3\</code>
                </div>
                <div>
                  <span className="text-zinc-200 font-bold">Standalone App:</span><br/>
                  <span className="text-zinc-300">Play directly without needing any DAW host!</span>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* TAB 2: HYBRID EVERSYNTH ARCHITECTURE */}
        {activeTab === 'specs' && (
          <div className="p-5 overflow-y-auto flex-1 flex flex-col gap-4 font-sans text-xs">
            <div className="p-3.5 rounded-lg bg-gradient-to-r from-cyan-950/30 via-zinc-900 to-zinc-900 border border-cyan-500/30 text-zinc-300">
              <span className="font-bold text-white text-sm flex items-center gap-2">
                <Globe className="w-4 h-4 text-cyan-400" />
                The EVERSYNTH Hybrid Plugin Paradigm: 100% 1:1 Accuracy
              </span>
              <p className="mt-1.5 leading-relaxed text-zinc-400 text-xs">
                In traditional plugin development, recreating complex web interfaces in native C++ graphics often results in subtle compromises.
                With the <strong>Hybrid Plugin architecture</strong>, your VST3 and AU plugins render the <strong>EXACT same React/Tailwind interface</strong> inside an ultra-high performance native WebView (WebKit on macOS / WebView2 on Windows), while all audio DSP, polyphony, filtering, modulation, and effects are calculated in the real-time C++ audio engine.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-3 gap-3.5">
              {/* Layer 1: Aesthetic (Web UI) */}
              <div className="p-3.5 rounded-lg bg-[#10121a] border border-[#1f2333] flex flex-col gap-2">
                <div className="flex items-center gap-2 text-cyan-400 font-bold uppercase tracking-wider text-[11px] pb-1 border-b border-[#1f2333]">
                  <Layers className="w-4 h-4" />
                  <span>1. Aesthetic: 100% 1:1 Web UI</span>
                </div>
                <ul className="space-y-1.5 text-zinc-300 font-mono text-[11px]">
                  <li>• <span className="text-white">Modular Sound Tech Logo:</span> Custom branding</li>
                  <li>• <span className="text-cyan-400">Radial Timbre DNA:</span> SVG LED arc gauges</li>
                  <li>• <span className="text-amber-400">Image Dropzone:</span> Drag-and-drop & color sampling</li>
                  <li>• <span className="text-pink-400">Morph Crossfader:</span> Smooth parameter interpolation</li>
                  <li>• <span className="text-purple-400">Typed BPM Input:</span> Direct keyboard editing & sync</li>
                  <li>• <span className="text-emerald-400">Effects Rack:</span> 6 studio DSP modules</li>
                </ul>
              </div>

              {/* Layer 2: Bi-Directional Bridge */}
              <div className="p-3.5 rounded-lg bg-[#10121a] border border-[#1f2333] flex flex-col gap-2">
                <div className="flex items-center gap-2 text-amber-400 font-bold uppercase tracking-wider text-[11px] pb-1 border-b border-[#1f2333]">
                  <Cpu className="w-4 h-4" />
                  <span>2. Hybrid Bridge (JUCE 8)</span>
                </div>
                <ul className="space-y-1.5 text-zinc-300 font-mono text-[11px]">
                  <li>• <span className="text-amber-300">juce::WebBrowserComponent:</span> Native container</li>
                  <li>• <span className="text-cyan-300">withResourceProvider:</span> 100% offline assets</li>
                  <li>• <span className="text-pink-300">withNativeFunction:</span> Zero-latency JS-C++ calls</li>
                  <li>• <span className="text-purple-300">60 FPS Oscilloscope FIFO:</span> Audio buffer stream</li>
                  <li>• <span className="text-emerald-300">DAW Automation Sync:</span> Live knob reflection</li>
                  <li>• <span className="text-white">Hardware Acceleration:</span> Metal & DirectX WebGL</li>
                </ul>
              </div>

              {/* Layer 3: Internals (C++ DSP Engine) */}
              <div className="p-3.5 rounded-lg bg-[#10121a] border border-[#1f2333] flex flex-col gap-2">
                <div className="flex items-center gap-2 text-emerald-400 font-bold uppercase tracking-wider text-[11px] pb-1 border-b border-[#1f2333]">
                  <Sliders className="w-4 h-4" />
                  <span>3. Internals: Real-Time C++ DSP</span>
                </div>
                <ul className="space-y-1.5 text-zinc-300 font-mono text-[11px]">
                  <li>• <span className="text-cyan-400">Sample Rates:</span> 44.1, 48, 88.2, 96, 192 kHz</li>
                  <li>• <span className="text-emerald-400">Voices:</span> 16-voice polyphony allocation</li>
                  <li>• <span className="text-amber-400">Filter:</span> 24dB resonant analog ladder SVF</li>
                  <li>• <span className="text-pink-400">Oscillators:</span> Dual detuned saw, 2-op FM, Wavetable</li>
                  <li>• <span className="text-purple-400">Effects Chain:</span> Delay, Reverb, Chorus, Phaser, Dist</li>
                  <li>• <span className="text-white">APVTS:</span> Thread-safe DAW automation storage</li>
                </ul>
              </div>
            </div>

            {/* Offline Verification */}
            <div className="p-3.5 rounded-lg bg-[#10121a] border border-[#1f2333] flex flex-col gap-2">
              <span className="font-bold text-zinc-200 uppercase tracking-wider text-[11px]">
                100% Offline & Host Integrated
              </span>
              <p className="text-zinc-400 text-xs leading-relaxed">
                The plugin does not require any internet connection or web server. All HTML, CSS, JavaScript, textures, and the Modular Sound Technologies custom logo are compiled directly into the plugin binary and local resource provider. In your DAW, it opens instantly, saves patch state with your DAW project session, and responds to MIDI keyboard controllers without any delay.
              </p>
            </div>
          </div>
        )}

        {/* TAB 3: CODE FILES VIEWER */}
        {activeTab === 'code' && (
          <div className="flex flex-1 min-h-0 overflow-hidden">
            {/* File List Sidebar */}
            <div className="w-64 bg-[#090b10] border-r border-[#232736] flex flex-col p-2 gap-1 overflow-y-auto shrink-0 font-mono text-xs">
              <span className="text-[10px] font-bold text-zinc-500 uppercase px-2 py-1">
                Project Files ({files.length})
              </span>
              {files.map((file, idx) => (
                <button
                  key={file.filename}
                  onClick={() => setSelectedFileIdx(idx)}
                  className={`text-left px-2.5 py-1.5 rounded flex flex-col gap-0.5 transition-all ${
                    selectedFileIdx === idx
                      ? 'bg-amber-500/15 border border-amber-500/40 text-amber-300'
                      : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/50 border border-transparent'
                  }`}
                >
                  <span className="font-bold truncate text-[11px]">{file.filename}</span>
                  <span className="text-[9px] text-zinc-500 truncate">{file.description}</span>
                </button>
              ))}
            </div>

            {/* Code View Area */}
            <div className="flex-1 flex flex-col min-w-0 bg-[#07080c] overflow-hidden">
              <div className="flex items-center justify-between px-4 py-2 bg-[#0d0f17] border-b border-[#232736] shrink-0">
                <div className="flex items-center gap-2">
                  <span className="font-mono font-bold text-xs text-cyan-300">
                    {currentFile.filename}
                  </span>
                  <span className="text-[10px] text-zinc-500 font-mono">({currentFile.language})</span>
                </div>
                <button
                  onClick={() => handleCopy(currentFile.code, currentFile.filename)}
                  className="flex items-center gap-1.5 px-2.5 py-1 rounded bg-[#181a24] hover:bg-[#222533] text-zinc-300 hover:text-white transition-all text-xs border border-[#272b3b]"
                >
                  {copiedKey === currentFile.filename ? (
                    <>
                      <Check className="w-3.5 h-3.5 text-emerald-400" />
                      <span className="text-emerald-400 font-semibold">Copied</span>
                    </>
                  ) : (
                    <>
                      <Copy className="w-3.5 h-3.5" />
                      <span>Copy Code</span>
                    </>
                  )}
                </button>
              </div>
              <pre className="p-4 overflow-auto flex-1 font-mono text-[11px] leading-relaxed text-zinc-300 select-text">
                <code>{currentFile.code}</code>
              </pre>
            </div>
          </div>
        )}

      </div>
    </div>
  );
};
