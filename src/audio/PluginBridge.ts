/**
 * PluginBridge.ts
 * ==============================================================================
 * Hybrid Plugin Audio Bridge (EVERSYNTH Architecture)
 * 
 * Provides bi-directional, real-time communication between the React Web UI
 * and the native C++ JUCE DSP Audio Processor when loaded inside a VST3, AU,
 * or Standalone plugin window via juce::WebBrowserComponent (JUCE 8).
 * 
 * When running in a standard web browser (AI Studio / Vite dev / web preview),
 * it seamlessly routes all calls to the Web Audio engine (WebAudioEngine).
 * ==============================================================================
 */

import { PatchParameters, ImageMetrics } from '../types/synth';
import { globalAudioEngine } from './WebAudioEngine';

export interface JuceBackend {
  emit: (eventName: string, data?: any) => void;
  addEventListener: (eventName: string, handler: (data: any) => void) => void;
  removeEventListener?: (eventName: string, handler: (data: any) => void) => void;
}

export interface JuceGlobal {
  backend?: JuceBackend;
  getNativeFunction?: (name: string) => (...args: any[]) => Promise<any>;
}

declare global {
  interface Window {
    __JUCE__?: JuceGlobal;
    Juce?: JuceGlobal;
    PhotoSynthNative?: {
      setParameter: (paramId: string, value: number) => void;
      noteOn: (note: number, velocity: number) => void;
      noteOff: (note: number) => void;
      setBpm: (bpm: number) => void;
      savePreset: (name: string, desc: string) => void;
      loadPreset: (name: string) => void;
      getWaveformData?: () => number[];
    };
    __PhotoSynthWebview?: {
      onWaveformUpdate: (samples: number[]) => void;
      onParameterUpdate: (paramId: string, value: number) => void;
      onBpmUpdate: (bpm: number) => void;
    };
  }
}

class HybridPluginBridge {
  private isConnectedToNative = false;
  private hasNativeWaveformStream = false;
  private nativeFunctions: Map<string, (...args: any[]) => Promise<any>> = new Map();
  private waveformListeners: Set<(data: Float32Array) => void> = new Set();
  private parameterListeners: Set<(paramId: string, value: number) => void> = new Set();
  private bpmListeners: Set<(bpm: number) => void> = new Set();
  private lastWaveform = new Float32Array(512);

  constructor() {
    this.detectEnvironment();
    this.setupGlobalReceivers();
  }

  /**
   * Detect whether running inside the JUCE native plugin WebView container.
   */
  public detectEnvironment(): boolean {
    if (typeof window === 'undefined') return false;

    const hasJuceBackend = !!(window.__JUCE__?.backend || window.Juce?.getNativeFunction);
    const hasCustomNative = !!window.PhotoSynthNative;
    const urlParams = new URLSearchParams(window.location.search);
    const forceHybrid = urlParams.get('hybrid') === '1' || urlParams.get('mode') === 'plugin';

    this.isConnectedToNative = hasJuceBackend || hasCustomNative || forceHybrid;
    return this.isConnectedToNative;
  }

  public isHybrid(): boolean {
    return this.isConnectedToNative;
  }

  public isNativeEnvironment(): boolean {
    return this.isConnectedToNative;
  }

  /**
   * Universal invoker for JUCE 8 Native Functions across all platforms.
   */
  public async callNative(name: string, ...args: any[]): Promise<any> {
    if (typeof window === 'undefined') return undefined;

    // 1. Cached function from getNativeFunction
    if (this.nativeFunctions.has(name)) {
      try {
        return await this.nativeFunctions.get(name)!(...args);
      } catch (err) {
        console.warn(`callNative(${name}) cached error:`, err);
      }
    }

    // 2. Dynamic lookup from window.__JUCE__.backend or window.Juce
    const juceBackend = (window as any).__JUCE__?.backend || (window as any).Juce?.backend;
    const juceRoot = (window as any).__JUCE__ || (window as any).Juce;

    const getNativeFn =
      (juceBackend && typeof juceBackend.getNativeFunction === 'function' && juceBackend.getNativeFunction.bind(juceBackend)) ||
      (juceRoot && typeof juceRoot.getNativeFunction === 'function' && juceRoot.getNativeFunction.bind(juceRoot));

    if (getNativeFn) {
      try {
        const fn = getNativeFn(name);
        if (typeof fn === 'function') {
          this.nativeFunctions.set(name, fn);
          return await fn(...args);
        }
      } catch (err) {
        console.warn(`callNative(${name}) getNativeFunction error:`, err);
      }
    }

    // 3. Direct method on juceBackend
    if (juceBackend && typeof juceBackend[name] === 'function') {
      try {
        return await juceBackend[name](...args);
      } catch (err) {
        console.warn(`callNative(${name}) direct backend method error:`, err);
      }
    }

    // 4. Custom bridge window.PhotoSynthNative
    const customNative = (window as any).PhotoSynthNative;
    if (customNative && typeof customNative[name] === 'function') {
      try {
        return await customNative[name](...args);
      } catch (err) {
        console.warn(`callNative(${name}) customNative error:`, err);
      }
    }

    return undefined;
  }

  /**
   * Initialize native function bindings if running in JUCE 8 WebView.
   */
  public async initNativeBridge(): Promise<void> {
    this.detectEnvironment();

    try {
      const fnNames = [
        'setParameter',
        'syncPatch',
        'openFileChooser',
        'noteOn',
        'noteOff',
        'setBpm',
        'savePreset',
        'loadPreset',
        'getWaveformData',
        'getInitialState',
      ];

      const juceBackend = (window as any).__JUCE__?.backend || (window as any).Juce?.backend;
      const juceRoot = (window as any).__JUCE__ || (window as any).Juce;
      const getNativeFn =
        (juceBackend && typeof juceBackend.getNativeFunction === 'function' && juceBackend.getNativeFunction.bind(juceBackend)) ||
        (juceRoot && typeof juceRoot.getNativeFunction === 'function' && juceRoot.getNativeFunction.bind(juceRoot));

      if (getNativeFn) {
        for (const name of fnNames) {
          try {
            const fn = getNativeFn(name);
            if (typeof fn === 'function') {
              this.nativeFunctions.set(name, fn);
            }
          } catch {}
        }
      }

      // If JUCE backend event bus is present
      if (juceBackend && typeof juceBackend.addEventListener === 'function') {
        juceBackend.addEventListener('waveform', (data: any) => {
          if (Array.isArray(data)) {
            const f32 = new Float32Array(data);
            this.lastWaveform = f32;
            this.waveformListeners.forEach((fn) => fn(f32));
          }
        });

        juceBackend.addEventListener('parameterChanged', (data: any) => {
          if (data && typeof data.id === 'string' && typeof data.value === 'number') {
            this.parameterListeners.forEach((fn) => fn(data.id, data.value));
          }
        });

        juceBackend.addEventListener('bpmChanged', (data: any) => {
          if (data && typeof data.bpm === 'number') {
            this.bpmListeners.forEach((fn) => fn(data.bpm));
          }
        });
      }
    } catch (err) {
      console.warn('Native bridge setup note:', err);
    }
  }

  /**
   * Set up global receiver object so C++ can evaluate JS directly.
   */
  private setupGlobalReceivers() {
    if (typeof window === 'undefined') return;

    window.__PhotoSynthWebview = {
      onWaveformUpdate: (samples: number[]) => {
        if (!samples || !samples.length) return;
        const f32 = new Float32Array(samples);
        this.lastWaveform = f32;
        this.hasNativeWaveformStream = true;
        this.waveformListeners.forEach((fn) => fn(f32));
      },
      onParameterUpdate: (paramId: string, value: number) => {
        this.parameterListeners.forEach((fn) => fn(paramId, value));
      },
      onBpmUpdate: (bpm: number) => {
        this.bpmListeners.forEach((fn) => fn(bpm));
      },
    };
  }

  // ============================================================================
  // Audio Control Methods (Hybrid Native + WebAudio Fallback)
  // ============================================================================

  /**
   * Send a parameter change to the C++ audio processor (and WebAudio engine).
   */
  public setParameter(paramId: string, value: number): void {
    // 1. Remap key for C++ APVTS
    const nativeParamId = paramId === 'cutoffOffset' ? 'cutoff' :
                          paramId === 'attackTime' ? 'attack' :
                          paramId === 'decayTime' ? 'decay' :
                          paramId === 'sustainLevel' ? 'sustain' :
                          paramId === 'releaseTime' ? 'release' :
                          paramId === 'masterVolume' ? 'volume' :
                          paramId === 'limiterEnabled' ? 'limiter' : paramId;

    if (this.isConnectedToNative) {
      this.callNative('setParameter', nativeParamId, value);
    }

    // 2. Always keep web engine in sync as well
    if (typeof value === 'number' && Number.isFinite(value)) {
      const webParamKey = paramId === 'cutoff' ? 'cutoffOffset' :
                          paramId === 'attack' ? 'attackTime' :
                          paramId === 'decay' ? 'decayTime' :
                          paramId === 'sustain' ? 'sustainLevel' :
                          paramId === 'release' ? 'releaseTime' :
                          paramId === 'volume' ? 'masterVolume' :
                          paramId === 'limiter' ? 'limiterEnabled' : paramId;
      globalAudioEngine.updateParameters({ [webParamKey]: value });
    }
  }

  /**
   * Request native OS file chooser to open an image (PNG/JPG/WebP) in VST/AU.
   */
  public async openImageFileChooser(slot: 'A' | 'B'): Promise<{ dataUrl: string; fileName: string } | null> {
    if (this.isConnectedToNative) {
      try {
        const res = await this.callNative('openFileChooser', slot);
        if (res && typeof res === 'object' && res.dataUrl) {
          return { dataUrl: res.dataUrl, fileName: res.fileName || `Image ${slot}` };
        }
      } catch (err) {
        console.warn('Native openFileChooser call failed:', err);
      }
    }
    return null;
  }

  /**
   * Sync image data and name to native C++ processor for state persistence.
   */
  public syncSlotImage(slot: 'A' | 'B', dataUrl: string, name: string): void {
    if (this.isConnectedToNative) {
      this.callNative('syncSlotImage', slot, dataUrl, name);
    }
  }

  /**
   * Send note on event.
   */
  public noteOn(midiNote: number, velocity: number = 0.85): void {
    if (this.isConnectedToNative) {
      this.callNative('noteOn', midiNote, velocity);
    } else {
      // Trigger WebAudio only in browser environment
      globalAudioEngine.noteOn(midiNote, velocity);
    }
  }

  /**
   * Send note off event.
   */
  public noteOff(midiNote: number): void {
    if (this.isConnectedToNative) {
      this.callNative('noteOff', midiNote);
    } else {
      // Web audio fallback in browser
      globalAudioEngine.noteOff(midiNote);
    }
  }

  /**
   * Send BPM update to C++ processor.
   */
  public setBpm(bpm: number): void {
    if (this.isConnectedToNative) {
      if (this.nativeFunctions.has('setBpm')) {
        this.nativeFunctions.get('setBpm')!(bpm).catch(() => {});
      } else if (window.PhotoSynthNative?.setBpm) {
        window.PhotoSynthNative.setBpm(bpm);
      } else if (window.__JUCE__?.backend?.emit) {
        window.__JUCE__.backend.emit('setBpm', { bpm });
      }
    }
  }

  /**
   * Save a user preset in the native engine preset store.
   */
  public savePreset(name: string, description: string): void {
    if (this.isConnectedToNative) {
      if (this.nativeFunctions.has('savePreset')) {
        this.nativeFunctions.get('savePreset')!(name, description).catch(() => {});
      } else if (window.PhotoSynthNative?.savePreset) {
        window.PhotoSynthNative.savePreset(name, description);
      } else if (window.__JUCE__?.backend?.emit) {
        window.__JUCE__.backend.emit('savePreset', { name, description });
      }
    }
  }

  /**
   * Load a preset from the native engine.
   */
  public loadPreset(name: string): void {
    if (this.isConnectedToNative) {
      if (this.nativeFunctions.has('loadPreset')) {
        this.nativeFunctions.get('loadPreset')!(name).catch(() => {});
      } else if (window.PhotoSynthNative?.loadPreset) {
        window.PhotoSynthNative.loadPreset(name);
      } else if (window.__JUCE__?.backend?.emit) {
        window.__JUCE__.backend.emit('loadPreset', { name });
      }
    }
  }

  /**
   * Fetch complete initial plugin state (BPM, parameters, and loaded slot images) from C++ processor.
   */
  public async getInitialState(): Promise<any> {
    if (this.isConnectedToNative) {
      try {
        return await this.callNative('getInitialState');
      } catch (err) {
        console.warn('getInitialState note:', err);
      }
    }
    return null;
  }

  /**
   * Sync complete patch parameters and image analysis to C++ DSP engine.
   */
  public syncFullPatch(patch: PatchParameters, metrics?: ImageMetrics | null): void {
    const archVal = (patch.engineType === 'digital_fm_bells' || patch.engineType === 'fm_square_bell') ? 1.0 :
                    (patch.engineType === 'hybrid_wavetable') ? 2.0 : 0.0;

    // Sync all primary parameters to the C++ APVTS
    const paramMap: Record<string, number> = {
      cutoff: patch.cutoffOffset ?? 2400,
      resonance: patch.resonance ?? 1.2,
      lfoRate: patch.lfoRate ?? 2.0,
      lfoDepth: patch.lfoDepth ?? 0.3,
      attack: patch.attackTime ?? 0.05,
      decay: patch.decayTime ?? 0.3,
      sustain: patch.sustainLevel ?? 0.7,
      release: patch.releaseTime ?? 0.4,
      drive: patch.physicalModel?.analogSaturationWarmth ?? 0.3,
      limiter: patch.limiterEnabled ? 1.0 : 0.0,
      volume: patch.masterVolume ?? -3.0,
      threshold: patch.threshold ?? -3.0,
      archetype: archVal,
    };

    if (patch.effects) {
      paramMap['delayMix'] = patch.effects.delay?.enabled ? patch.effects.delay.intensity : 0.0;
      paramMap['reverbMix'] = patch.effects.reverb?.enabled ? patch.effects.reverb.intensity : 0.0;
      paramMap['chorusMix'] = patch.effects.chorus?.enabled ? patch.effects.chorus.intensity : 0.0;
      paramMap['phaserMix'] = patch.effects.phaser?.enabled ? patch.effects.phaser.intensity : 0.0;
      paramMap['flangerMix'] = patch.effects.flanger?.enabled ? patch.effects.flanger.intensity : 0.0;
      paramMap['distortionMix'] = patch.effects.distortion?.enabled ? patch.effects.distortion.intensity : 0.0;
    }

    if (this.isConnectedToNative && this.nativeFunctions.has('syncPatch')) {
      this.nativeFunctions.get('syncPatch')!(paramMap).catch(() => {});
    }

    // Keep web audio in sync only if running in web browser fallback
    if (!this.isConnectedToNative) {
      globalAudioEngine.updateParameters(patch);
    }
  }

  /**
   * Panic button: kill all voices.
   */
  public panic(): void {
    for (let note = 0; note < 128; note++) {
      this.noteOff(note);
    }
    globalAudioEngine.panic();
  }

  // ============================================================================
  // Oscilloscope & Event Listeners
  // ============================================================================

  public onWaveform(listener: (data: Float32Array) => void): () => void {
    this.waveformListeners.add(listener);
    return () => this.waveformListeners.delete(listener);
  }

  public onParameterChange(listener: (paramId: string, value: number) => void): () => void {
    this.parameterListeners.add(listener);
    return () => this.parameterListeners.delete(listener);
  }

  public onBpmChange(listener: (bpm: number) => void): () => void {
    this.bpmListeners.add(listener);
    return () => this.bpmListeners.delete(listener);
  }

  /**
   * Get latest waveform data (either from native engine buffer or web audio analyser).
   */
  public getWaveformData(destinationArray: Uint8Array): void {
    if (this.hasNativeWaveformStream && this.lastWaveform.length > 0) {
      // Map float [-1.0, 1.0] to uint8 [0, 255]
      const len = Math.min(destinationArray.length, this.lastWaveform.length);
      for (let i = 0; i < len; i++) {
        const norm = (this.lastWaveform[i] + 1.0) * 0.5;
        destinationArray[i] = Math.max(0, Math.min(255, Math.round(norm * 255)));
      }
    } else {
      globalAudioEngine.getWaveformData(destinationArray);
    }
  }
}

export const pluginBridge = new HybridPluginBridge();
