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

    const hasJuceBackend = !!((window as any).__JUCE__?.backend || (window as any).Juce?.getNativeFunction || (window as any).__JUCE__?.getNativeFunction);
    const hasCustomNative = !!(window as any).PhotoSynthNative || !!(window as any).__PHOTO_SYNTH_HYBRID__ || !!(window as any).__PHOTO_SYNTH_NATIVE_HOST__;
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

  // ==========================================================================
  // JUCE 8 native-function invocation
  // --------------------------------------------------------------------------
  // JUCE 8 injects only `window.__JUCE__.backend` (emitEvent / addEventListener).
  // `getNativeFunction` lives in JUCE's optional frontend library
  // (juce_gui_extra/native/javascript/index.js), which this app does NOT bundle,
  // so we speak the underlying protocol directly (identical to that library):
  //   JS  -> C++ : backend.emitEvent("__juce__invoke", { name, params: [...], resultId })
  //   C++ -> JS  : backend event "__juce__complete" with { promiseId, result }
  // ==========================================================================
  private nextPromiseId = 0;
  private pendingCalls = new Map<number, { resolve: (v: any) => void; timer: ReturnType<typeof setTimeout> }>();
  private completionListenerBackend: any = null;
  public static NATIVE_CALL_TIMEOUT_MS = 4000;
  /** Calls that legitimately take longer (user browsing for a file, large images). */
  public static NATIVE_CALL_TIMEOUT_OVERRIDES_MS: Record<string, number> = {
    openFileChooser: 10 * 60 * 1000,
    syncSlotImage: 15000,
    getInitialState: 15000,
  };

  private timeoutFor(name: string): number {
    return HybridPluginBridge.NATIVE_CALL_TIMEOUT_OVERRIDES_MS[name] ?? HybridPluginBridge.NATIVE_CALL_TIMEOUT_MS;
  }

  private getJuceBackend(): any {
    if (typeof window === 'undefined') return null;
    return (window as any).__JUCE__?.backend || (window as any).Juce?.backend || null;
  }

  private getNativeFunctionFactory(): ((name: string) => any) | null {
    if (typeof window === 'undefined') return null;
    const backend = this.getJuceBackend();
    const root = (window as any).__JUCE__ || (window as any).Juce;
    if (backend && typeof backend.getNativeFunction === 'function') return backend.getNativeFunction.bind(backend);
    if (root && typeof root.getNativeFunction === 'function') return root.getNativeFunction.bind(root);
    return null;
  }

  /** Register (once per backend object) the "__juce__complete" listener that resolves pending calls. */
  private ensureCompletionListener(backend: any): void {
    if (!backend || this.completionListenerBackend === backend || typeof backend.addEventListener !== 'function') return;
    this.completionListenerBackend = backend;
    backend.addEventListener('__juce__complete', (payload: any) => {
      const id = payload?.promiseId;
      const pending = this.pendingCalls.get(id);
      if (pending) {
        clearTimeout(pending.timer);
        this.pendingCalls.delete(id);
        pending.resolve(payload?.result);
      }
    });
  }

  /** True when a JUCE 8 backend that can carry native-function calls is present. */
  public hasNativeTransport(): boolean {
    const backend = this.getJuceBackend();
    return !!(this.getNativeFunctionFactory() || (backend && typeof backend.emitEvent === 'function'));
  }

  /**
   * Invoke a C++ native function registered with WebBrowserComponent::Options::withNativeFunction.
   * Resolves with the C++ completion value, or `undefined` on timeout / when no backend exists.
   * Never rejects.
   */
  public callNative(name: string, ...args: any[]): Promise<any> {
    if (typeof window === 'undefined') return Promise.resolve(undefined);

    // 1. Prefer JUCE's own frontend library if the page happens to provide it.
    const factory = this.getNativeFunctionFactory();
    if (factory) {
      try {
        let fn = this.nativeFunctions.get(name);
        if (!fn) {
          fn = factory(name);
          if (typeof fn === 'function') this.nativeFunctions.set(name, fn);
        }
        if (typeof fn === 'function') {
          return this.withTimeout(Promise.resolve(fn(...args)), name);
        }
      } catch (err) {
        console.warn(`callNative(${name}) getNativeFunction error:`, err);
      }
    }

    // 2. Raw JUCE 8 invoke protocol over the injected backend.
    const backend = this.getJuceBackend();
    if (backend && typeof backend.emitEvent === 'function') {
      this.ensureCompletionListener(backend);
      const resultId = this.nextPromiseId++;
      return new Promise((resolve) => {
        const timer = setTimeout(() => {
          if (this.pendingCalls.delete(resultId)) {
            console.warn(`callNative(${name}) timed out after ${this.timeoutFor(name)} ms`);
            resolve(undefined);
          }
        }, this.timeoutFor(name));
        this.pendingCalls.set(resultId, { resolve, timer });
        try {
          backend.emitEvent('__juce__invoke', { name, params: args, resultId });
        } catch (err) {
          clearTimeout(timer);
          this.pendingCalls.delete(resultId);
          console.warn(`callNative(${name}) emitEvent error:`, err);
          resolve(undefined);
        }
      });
    }

    // 3. Legacy custom bridge.
    const customNative = (window as any).PhotoSynthNative;
    if (customNative && typeof customNative[name] === 'function') {
      try {
        return Promise.resolve(customNative[name](...args)).catch(() => undefined);
      } catch {
        return Promise.resolve(undefined);
      }
    }

    console.warn(`callNative(${name}): no JUCE backend available - call dropped`);
    return Promise.resolve(undefined);
  }

  private withTimeout<T>(p: Promise<T>, name: string): Promise<T | undefined> {
    return new Promise((resolve) => {
      const timer = setTimeout(() => {
        console.warn(`callNative(${name}) timed out after ${this.timeoutFor(name)} ms`);
        resolve(undefined);
      }, this.timeoutFor(name));
      p.then((v) => { clearTimeout(timer); resolve(v); }, (err) => {
        clearTimeout(timer);
        console.warn(`callNative(${name}) error:`, err);
        resolve(undefined);
      });
    });
  }

  /**
   * Initialize native bindings if running in JUCE 8 WebView.
   */
  public async initNativeBridge(): Promise<void> {
    this.detectEnvironment();

    // If running in a hybrid plugin host, wait for window.__JUCE__ to be injected
    if (this.isConnectedToNative) {
      for (let attempt = 0; attempt < 25 && !this.hasNativeTransport(); ++attempt) {
        await new Promise((r) => setTimeout(r, 40));
        this.detectEnvironment();
      }
    }

    try {
      const juceBackend = this.getJuceBackend();
      if (juceBackend) this.ensureCompletionListener(juceBackend);

      // If JUCE backend event bus is present
      if (juceBackend && typeof juceBackend.addEventListener === 'function' && !this.eventListenersAttached) {
        this.eventListenersAttached = true;
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
  private eventListenersAttached = false;

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

    // 2. Keep the browser WebAudio engine in sync only when it is the active engine
    //    (web preview). In plugin mode the C++ engine is the sole sound source.
    if (!this.isConnectedToNative && typeof value === 'number' && Number.isFinite(value)) {
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
  public syncSlotImage(slot: 'A' | 'B', dataUrl: string, name: string): Promise<any> {
    if (this.isConnectedToNative && typeof dataUrl === 'string' && dataUrl.startsWith('data:')) {
      return this.callNative('syncSlotImage', slot, dataUrl, name || `Image ${slot}`);
    }
    return Promise.resolve(undefined);
  }

  /**
   * Send note on event.
   */
  public noteOn(midiNote: number, velocity: number = 0.85): void {
    if (this.isConnectedToNative) {
      // C++ expects 0..1; accept MIDI-style 0..127 too.
      const v = velocity > 1 ? velocity / 127 : velocity;
      this.callNative('noteOn', Math.round(midiNote), Math.max(0, Math.min(1, v)));
    }
    // Browser WebAudio engine plays ONLY outside the plugin. Inside a DAW/standalone
    // plugin, on-screen keys must go through the same C++ engine as host MIDI,
    // otherwise two different synths are heard.
    if (!this.isConnectedToNative) {
      try {
        globalAudioEngine.noteOn(midiNote, velocity);
      } catch {}
    }
  }

  /**
   * Send note off event.
   */
  public noteOff(midiNote: number): void {
    if (this.isConnectedToNative) {
      this.callNative('noteOff', Math.round(midiNote));
    }
    if (!this.isConnectedToNative) {
      try {
        globalAudioEngine.noteOff(midiNote);
      } catch {}
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
   * Build the C++ APVTS parameter map (APVTS units) for a photo-derived patch.
   * Exposed for tests. Effect mappings mirror WebAudioEngine.updateParameters.
   */
  public buildNativePatchMap(patch: PatchParameters, metrics?: ImageMetrics | null, morph?: number): Record<string, number> {
    const num = (v: any, d: number) => (typeof v === 'number' && Number.isFinite(v) ? v : d);
    const clamp01 = (v: number) => Math.max(0, Math.min(1, v));
    const archVal = (patch.engineType === 'digital_fm_bells' || patch.engineType === 'fm_square_bell') ? 1.0 :
                    (patch.engineType === 'hybrid_wavetable') ? 2.0 : 0.0;

    const paramMap: Record<string, number> = {
      cutoff: num(patch.cutoffOffset, 2400),
      resonance: num(patch.resonance, 1.2),
      lfoRate: num(patch.lfoRate, 2.0),
      lfoDepth: num(patch.lfoDepth, 0.25),
      attack: num(patch.attackTime, 0.03),
      decay: num(patch.decayTime, 0.35),
      sustain: num(patch.sustainLevel, 0.7),
      release: num(patch.releaseTime, 0.45),
      drive: num(patch.physicalModel?.analogSaturationWarmth, 0.0),
      limiter: patch.limiterEnabled === false ? 0.0 : 1.0,
      volume: num(patch.masterVolume, -3.0),
      threshold: num(patch.threshold, -3.0),
      archetype: archVal,
      color: clamp01(num(metrics?.timbreDna, 0.5)) * 360.0,
      brightness: clamp01(num(metrics?.brightness, 0.5)) * 100.0,
      saturation: clamp01(num(metrics?.saturation, 0.5)) * 100.0,
      complexity: clamp01(num(metrics?.complexity, 0.2)) * 100.0,
    };

    if (typeof morph === 'number' && Number.isFinite(morph)) paramMap.morph = clamp01(morph);

    const fx = patch.effects;
    if (fx) {
      const p1 = (e: any, d: number) => num(e?.param1, num(e?.intensity, d));
      const p2 = (e: any, d: number) => num(e?.param2, num(e?.intensity, d));
      const on = (e: any) => (e ? !!e.enabled : true);

      const delayOn = on(fx.delay);
      paramMap.delayEnabled = delayOn ? 1 : 0;
      paramMap.delayTime = Math.max(10, Math.min(1500, (0.05 + p1(fx.delay, 0.5) * 0.75) * 1000));
      paramMap.delayFeedback = Math.max(0, Math.min(0.95, p2(fx.delay, 0.5) * 0.85));
      paramMap.delayMix = delayOn ? clamp01(Math.max(num((patch as any).delayMix, 0), p1(fx.delay, 0.5) * 0.5)) : 0;

      const reverbOn = on(fx.reverb);
      paramMap.reverbEnabled = reverbOn ? 1 : 0;
      paramMap.reverbMix = reverbOn ? clamp01(Math.max(num((patch as any).reverbMix, 0), p1(fx.reverb, 0.6) * 0.8)) : 0;
      paramMap.reverbDecay = Math.max(0.2, Math.min(10, 0.5 + p2(fx.reverb, 0.6) * 5.5));

      const chorusOn = on(fx.chorus);
      paramMap.chorusEnabled = chorusOn ? 1 : 0;
      paramMap.chorusMix = chorusOn ? clamp01(p2(fx.chorus, 0.45) * 0.45) : 0;

      const phaserOn = on(fx.phaser);
      paramMap.phaserEnabled = phaserOn ? 1 : 0;
      paramMap.phaserMix = phaserOn ? clamp01(p2(fx.phaser, 0.5) * 0.45) : 0;

      const flangerOn = on(fx.flanger);
      paramMap.flangerEnabled = flangerOn ? 1 : 0;
      paramMap.flangerMix = flangerOn ? clamp01(p2(fx.flanger, 0.4) * 0.45) : 0;

      const distOn = on(fx.distortion);
      paramMap.distortionEnabled = distOn ? 1 : 0;
      paramMap.distortionMix = distOn ? clamp01(p1(fx.distortion, 0.35) * 0.4) : 0;
    }
    return paramMap;
  }

  /**
   * Sync complete patch parameters and image analysis to C++ DSP engine.
   * In native mode this ALWAYS sends (no dependency on getNativeFunction).
   */
  public syncFullPatch(patch: PatchParameters, metrics?: ImageMetrics | null, morph?: number): Promise<any> {
    if (!patch) return Promise.resolve(undefined);

    if (this.isConnectedToNative) {
      return this.callNative('syncPatch', this.buildNativePatchMap(patch, metrics, morph));
    }

    // Web preview: the browser WebAudio engine is the active engine.
    globalAudioEngine.updateParameters(patch);
    return Promise.resolve(undefined);
  }

  /**
   * Panic button: kill all voices.
   */
  public panic(): void {
    for (let note = 0; note < 128; note++) {
      this.noteOff(note);
    }
    if (!this.isConnectedToNative) {
      globalAudioEngine.panic();
    }
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

  private waveformRequestInFlight = false;
  private lastWaveformRequest = 0;

  /** Poll the C++ oscilloscope buffer (~30 Hz max, one request in flight). */
  private requestNativeWaveform(numSamples: number): void {
    const now = Date.now();
    if (this.waveformRequestInFlight || now - this.lastWaveformRequest < 33) return;
    this.waveformRequestInFlight = true;
    this.lastWaveformRequest = now;
    this.callNative('getWaveformData', Math.max(16, Math.min(512, numSamples)))
      .then((arr) => {
        if (Array.isArray(arr) && arr.length > 0) {
          this.lastWaveform = new Float32Array(arr);
          this.hasNativeWaveformStream = true;
        }
      })
      .finally(() => { this.waveformRequestInFlight = false; });
  }

  /**
   * Get latest waveform data (native C++ scope buffer in plugin mode, web audio analyser otherwise).
   */
  public getWaveformData(destinationArray: Uint8Array): void {
    if (this.isConnectedToNative) {
      this.requestNativeWaveform(destinationArray.length);
      if (!this.hasNativeWaveformStream) {
        destinationArray.fill(128); // silence until the first native buffer arrives
        return;
      }
    }
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
