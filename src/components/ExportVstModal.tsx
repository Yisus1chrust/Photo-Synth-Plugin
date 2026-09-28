import React from 'react';
import { X, Cpu, Layers, Sliders, ExternalLink, Activity, Sparkles, Music } from 'lucide-react';
import { GUMROAD_STORE_URL } from '../services/licenseService';

interface ExportVstModalProps {
  isOpen: boolean;
  onClose: () => void;
}

export const ExportVstModal: React.FC<ExportVstModalProps> = ({ isOpen, onClose }) => {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/85 backdrop-blur-md p-3 sm:p-6 animate-in fade-in duration-200 select-none">
      <div className="relative w-full max-w-2xl bg-[#0e1219] border border-[#273042] rounded-xl shadow-[0_20px_60px_rgba(0,0,0,0.9)] flex flex-col overflow-hidden text-zinc-200 font-mono">
        
        {/* Top Header */}
        <div className="flex items-center justify-between px-5 py-3.5 bg-gradient-to-r from-[#121622] via-[#171d2b] to-[#121622] border-b border-[#202738] shrink-0">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded-md bg-white p-1 flex items-center justify-center border border-zinc-300 shadow-sm">
              <img
                src="/MODULARSOUNDTECHNOLOGIES.png"
                alt="Modular Sound Technologies"
                className="max-w-full max-h-full object-contain"
              />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <span className="font-extrabold tracking-wider text-sm text-zinc-100 uppercase">
                  MODULAR SOUND TECHNOLOGIES
                </span>
                <span className="text-[10px] font-bold px-1.5 py-0.5 rounded bg-cyan-950/80 border border-cyan-500/50 text-cyan-300">
                  v1.0.0
                </span>
              </div>
              <p className="text-[10px] text-zinc-400">
                Photo-Synth — Audiovisual Polyphonic Synthesizer
              </p>
            </div>
          </div>

          <button
            onClick={onClose}
            className="p-1 rounded-md text-zinc-400 hover:text-white hover:bg-zinc-800 transition-all cursor-pointer"
            title="Close"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Content Body */}
        <div className="p-5 sm:p-6 space-y-4 overflow-y-auto max-h-[78vh] text-xs">
          {/* Main Instrument Overview */}
          <div className="p-3.5 rounded-lg bg-[#131924] border border-[#21293a] flex items-start gap-3">
            <div className="p-2 rounded bg-cyan-500/10 text-cyan-400 shrink-0">
              <Sparkles className="w-4 h-4" />
            </div>
            <div className="space-y-1">
              <h4 className="font-bold text-zinc-100 text-xs uppercase tracking-wide">
                Image-Driven Synthesis Engine
              </h4>
              <p className="text-zinc-400 text-[11px] leading-relaxed">
                Photo-Synth analyzes photographic hue, luminance, saturation, and visual edge complexity to sculpt polyphonic synthesizer patches in real time.
              </p>
            </div>
          </div>

          {/* Plugin Formats & Compatibility */}
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
            <div className="p-3 rounded-lg bg-[#111622] border border-[#1e2536] space-y-1">
              <div className="flex items-center gap-1.5 text-amber-400 font-bold text-[11px] uppercase">
                <Music className="w-3.5 h-3.5" />
                <span>Plugin Formats</span>
              </div>
              <p className="text-zinc-300 text-[11px] font-mono">
                • VST3 (64-bit)<br />
                • Audio Unit / AU (macOS)<br />
                • Standalone Application
              </p>
            </div>

            <div className="p-3 rounded-lg bg-[#111622] border border-[#1e2536] space-y-1">
              <div className="flex items-center gap-1.5 text-cyan-400 font-bold text-[11px] uppercase">
                <Cpu className="w-3.5 h-3.5" />
                <span>Host Compatibility</span>
              </div>
              <p className="text-zinc-300 text-[11px] font-mono">
                • Ableton Live · Logic Pro<br />
                • FL Studio · Reaper · Cubase<br />
                • Bitwig Studio · Studio One
              </p>
            </div>
          </div>

          {/* Synthesis & DSP Specifications */}
          <div className="p-3.5 rounded-lg bg-[#111622] border border-[#1e2536] space-y-2.5">
            <div className="flex items-center gap-1.5 text-emerald-400 font-bold text-[11px] uppercase">
              <Sliders className="w-3.5 h-3.5" />
              <span>Technical Specifications</span>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-3 gap-2.5 text-[11px] text-zinc-400">
              <div className="space-y-0.5">
                <span className="text-zinc-200 font-bold block">Polyphony</span>
                <span>16-voice dynamic allocation with voice stealing</span>
              </div>

              <div className="space-y-0.5">
                <span className="text-zinc-200 font-bold block">Resonant Filter</span>
                <span>24dB/oct State-Variable (Lowpass, Bandpass, Highpass)</span>
              </div>

              <div className="space-y-0.5">
                <span className="text-zinc-200 font-bold block">DSP Multi-FX</span>
                <span>Ping-Pong Delay, Reverb, Chorus, Phaser, Flanger, Drive</span>
              </div>
            </div>
          </div>

          {/* Timbre DNA Mapping Grid */}
          <div className="p-3.5 rounded-lg bg-[#111622] border border-[#1e2536] space-y-2">
            <div className="flex items-center gap-1.5 text-pink-400 font-bold text-[11px] uppercase">
              <Layers className="w-3.5 h-3.5" />
              <span>Timbre DNA Mapping Matrix</span>
            </div>

            <div className="grid grid-cols-2 gap-2 text-[10px] text-zinc-400">
              <div className="bg-[#0b0e15] p-2 rounded border border-[#1a2130]">
                <span className="text-pink-300 font-bold block">Color / Hue</span>
                Harmonic balance & oscillator spread
              </div>
              <div className="bg-[#0b0e15] p-2 rounded border border-[#1a2130]">
                <span className="text-amber-300 font-bold block">Brightness</span>
                Filter cutoff frequency & top-end air
              </div>
              <div className="bg-[#0b0e15] p-2 rounded border border-[#1a2130]">
                <span className="text-purple-300 font-bold block">Saturation</span>
                Filter resonance & harmonic saturation
              </div>
              <div className="bg-[#0b0e15] p-2 rounded border border-[#1a2130]">
                <span className="text-cyan-300 font-bold block">Complexity</span>
                Envelope attack speed & transient snap
              </div>
            </div>
          </div>

          {/* Footer Action */}
          <div className="pt-2 flex items-center justify-between border-t border-[#1e2536]">
            <span className="text-[10px] text-zinc-500">
              © 2026 Modular Sound Technologies
            </span>

            <a
              href={GUMROAD_STORE_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="vst-btn-3d text-cyan-400 hover:text-cyan-300 text-[11px] font-bold px-3 py-1.5 rounded flex items-center gap-1.5 border border-cyan-500/40"
            >
              <span>Visit Gumroad Store</span>
              <ExternalLink className="w-3 h-3" />
            </a>
          </div>
        </div>

      </div>
    </div>
  );
};
