import { PatchParameters } from '../types/synth';

export interface PolyVoice {
  note: number;
  velocity: number;
  osc1: OscillatorNode;
  osc2: OscillatorNode;
  subOsc?: OscillatorNode;
  fmModulator?: OscillatorNode;
  fmGain?: GainNode;
  hammerClick?: OscillatorNode;
  hammerGain?: GainNode;
  shaperNode?: WaveShaperNode;
  filter: BiquadFilterNode;
  ampGain: GainNode;
  startTime: number;
}

export const DEFAULT_PATCH_PARAMS: PatchParameters = {
  cutoffOffset: 2400,
  resonance: 1.2,
  lfoRate: 2.0,
  lfoDepth: 0.25,
  masterVolume: -3.0,
  limiterEnabled: true,
  threshold: -1.0,
  attackTime: 0.03,
  decayTime: 0.35,
  sustainLevel: 0.7,
  releaseTime: 0.45,
  engineType: 'hybrid_wavetable',
  fmRatio: 2.0,
  fmAmount: 0.25,
  detuneCents: 6.0,
  unisonVoices: 2,
  subOscLevel: 0.25,
  delayMix: 0.2,
  reverbMix: 0.3,
  opticalFocusDepth: 0.5,
  gridSymmetryDensity: 0.5,
  lightAzimuthAngle: 180,
  lightElevationAngle: 45,
  chromaticClash: 0.2,
  semanticDensityWeight: 0.5,
  temporalEraVal: 0.5,
};

/**
 * True when the UI is hosted inside the JUCE 8 WebBrowserComponent (VST3 / AU /
 * Standalone plugin). In that case ALL audio must come from the native C++ engine,
 * so this browser engine must never create an AudioContext or emit sound.
 * Checked on every call because window.__JUCE__ can be injected after module load.
 * (Kept local - importing PluginBridge here would create a circular import.)
 */
export function isPluginHostEnvironment(): boolean {
  if (typeof window === 'undefined') return false;
  const w = window as any;
  if (w.__PHOTO_SYNTH_HYBRID__ || w.__PHOTO_SYNTH_NATIVE_HOST__ || w.PhotoSynthNative) return true;
  if (w.__JUCE__?.backend || w.__JUCE__?.getNativeFunction || w.Juce?.getNativeFunction) return true;
  try {
    const q = new URLSearchParams(window.location.search);
    return q.get('hybrid') === '1' || q.get('mode') === 'plugin';
  } catch {
    return false;
  }
}

export class WebAudioEngine {
  private ctx: AudioContext | null = null;
  private masterGain: GainNode | null = null;
  private limiter: DynamicsCompressorNode | null = null;
  private delayNode: DelayNode | null = null;
  private delayFeedbackGain: GainNode | null = null;
  private delayMixGain: GainNode | null = null;
  private reverbNode: ConvolverNode | null = null;
  private reverbMixGain: GainNode | null = null;
  private chorusMixGain: GainNode | null = null;
  private phaserMixGain: GainNode | null = null;
  private flangerMixGain: GainNode | null = null;
  private distortionMixGain: GainNode | null = null;
  private masterSoftClipper: WaveShaperNode | null = null;
  private dcBlockerNode: IIRFilterNode | null = null;
  private analyser: AnalyserNode | null = null;
  private globalLfo: OscillatorNode | null = null;
  private globalLfoGain: GainNode | null = null;
  private lastReverbDecay: number = 2.5;

  // Physical Modeling & Acoustic Resonance Nodes
  private bodyFilter1: BiquadFilterNode | null = null;
  private bodyFilter2: BiquadFilterNode | null = null;
  private bodyFilter3: BiquadFilterNode | null = null;
  private bodyMixGain: GainNode | null = null;
  private tapeFlutterDelay: DelayNode | null = null;
  private tapeFlutterLfo: OscillatorNode | null = null;
  private tapeFlutterGain: GainNode | null = null;
  private saturationShaper: WaveShaperNode | null = null;

  // Advanced Background Audio Engines (Panner, Ring Mod, Optical Depth, Sequencer & Physics)
  private pannerNode: StereoPannerNode | null = null;
  private ringModOsc: OscillatorNode | null = null;
  private ringModGain: GainNode | null = null;

  private activeVoices: Map<number, PolyVoice> = new Map();
  private fadingVoices: Set<PolyVoice> = new Set();
  private patchParams: PatchParameters = { ...DEFAULT_PATCH_PARAMS };
  private isInitialized = false;

  constructor() {
    // AudioContext created lazily on user interaction
  }

  /**
   * Safe AudioParam helper: prevents non-finite values from throwing in setTargetAtTime
   */
  private safeSetTargetAtTime(
    param: AudioParam | null | undefined,
    value: number,
    startTime: number,
    timeConstant: number,
    fallback = 0
  ): void {
    if (!param) return;
    const safeVal = Number.isFinite(value) ? value : fallback;
    const safeStart = Number.isFinite(startTime) ? startTime : (this.ctx?.currentTime ?? 0);
    const safeTc = Number.isFinite(timeConstant) && timeConstant > 0 ? timeConstant : 0.05;
    try {
      param.setTargetAtTime(safeVal, safeStart, safeTc);
    } catch {
      // AudioParam scheduling safety
    }
  }

  /**
   * Safe AudioParam helper: prevents non-finite values from throwing in setValueAtTime
   */
  private safeSetValueAtTime(
    param: AudioParam | null | undefined,
    value: number,
    time: number,
    fallback = 0
  ): void {
    if (!param) return;
    const safeVal = Number.isFinite(value) ? value : fallback;
    const safeTime = Number.isFinite(time) ? time : (this.ctx?.currentTime ?? 0);
    try {
      param.setValueAtTime(safeVal, safeTime);
    } catch {}
  }

  /**
   * Safe AudioParam helper: prevents non-finite values from throwing in linearRampToValueAtTime
   */
  private safeLinearRampToValueAtTime(
    param: AudioParam | null | undefined,
    value: number,
    endTime: number,
    fallback = 0
  ): void {
    if (!param) return;
    const safeVal = Number.isFinite(value) ? value : fallback;
    const safeEnd = Number.isFinite(endTime) ? endTime : (this.ctx?.currentTime ?? 0) + 0.01;
    try {
      param.linearRampToValueAtTime(safeVal, safeEnd);
    } catch {}
  }

  /**
   * Safe AudioParam helper: prevents non-finite values from throwing in exponentialRampToValueAtTime
   */
  private safeExponentialRampToValueAtTime(
    param: AudioParam | null | undefined,
    value: number,
    endTime: number,
    fallback = 0.001
  ): void {
    if (!param) return;
    const safeVal = Number.isFinite(value) && value > 0 ? value : fallback;
    const safeEnd = Number.isFinite(endTime) ? endTime : (this.ctx?.currentTime ?? 0) + 0.01;
    try {
      param.exponentialRampToValueAtTime(safeVal, safeEnd);
    } catch {}
  }

  public async initAudio(): Promise<void> {
    // Plugin/DAW mode: the native C++ engine is the only sound source. Never create
    // an AudioContext here (it would play a second, different synth in the WebView).
    if (isPluginHostEnvironment()) return;
    if (this.isInitialized && this.ctx && this.ctx.state === 'running') return;

    try {
      const AudioCtx = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      if (!AudioCtx) return;
      if (!this.ctx) {
        this.ctx = new AudioCtx();
      }

      if (this.ctx.state === 'suspended') {
        // Non-blocking resume so it never hangs or locks the UI thread if gestures are deferred
        this.ctx.resume().catch(() => {});
      }
    } catch (err) {
      console.warn('AudioContext init warning (native environment active):', err);
      return;
    }

    if (this.isInitialized) return;

    // 1. Analyser Node
    this.analyser = this.ctx.createAnalyser();
    this.analyser.fftSize = 1024;
    this.analyser.smoothingTimeConstant = 0.8;

    // 2. Master Gain & Limiter & Master Soft-Clipper
    this.masterGain = this.ctx.createGain();
    this.masterGain.gain.value = 0.5;

    this.limiter = this.ctx.createDynamicsCompressor();
    this.limiter.threshold.value = -1.0; // dB
    this.limiter.knee.value = 12.0;      // Smooth soft musical knee
    this.limiter.ratio.value = 4.0;      // Gentle transparent compression
    this.limiter.attack.value = 0.01;    // 10ms smooth transparent attack
    this.limiter.release.value = 0.15;   // 150ms smooth recovery

    this.masterSoftClipper = this.ctx.createWaveShaper();
    this.masterSoftClipper.curve = this.makeMasterSoftClipCurve() as unknown as Float32Array<ArrayBuffer>;
    this.masterSoftClipper.oversample = '2x';

    // 3. Effects: Delay & Reverb
    this.delayNode = this.ctx.createDelay(1.0);
    this.delayNode.delayTime.value = 0.35; // 350ms delay
    this.delayFeedbackGain = this.ctx.createGain();
    this.delayFeedbackGain.gain.value = 0.35;
    this.delayMixGain = this.ctx.createGain();
    this.delayMixGain.gain.value = 0.2;

    this.delayNode.connect(this.delayFeedbackGain);
    this.delayFeedbackGain.connect(this.delayNode);
    this.delayNode.connect(this.delayMixGain);

    // Synthetic Reverb Impulse
    this.reverbNode = this.ctx.createConvolver();
    this.reverbNode.buffer = this.createReverbImpulse(2.5, 2.0);
    this.reverbMixGain = this.ctx.createGain();
    this.reverbMixGain.gain.value = 0.3;

    // Additional Effect Mix Gains (Chorus, Phaser, Flanger, Distortion)
    this.chorusMixGain = this.ctx.createGain();
    this.chorusMixGain.gain.value = 0.2;

    this.phaserMixGain = this.ctx.createGain();
    this.phaserMixGain.gain.value = 0.2;

    this.flangerMixGain = this.ctx.createGain();
    this.flangerMixGain.gain.value = 0.2;

    this.distortionMixGain = this.ctx.createGain();
    this.distortionMixGain.gain.value = 0.15;

    // Body Resonance Filters (Physical Cavity Resonators)
    this.bodyFilter1 = this.ctx.createBiquadFilter();
    this.bodyFilter1.type = 'bandpass';
    this.bodyFilter1.frequency.value = 180;
    this.bodyFilter1.Q.value = 4.0;

    this.bodyFilter2 = this.ctx.createBiquadFilter();
    this.bodyFilter2.type = 'bandpass';
    this.bodyFilter2.frequency.value = 520;
    this.bodyFilter2.Q.value = 4.0;

    this.bodyFilter3 = this.ctx.createBiquadFilter();
    this.bodyFilter3.type = 'bandpass';
    this.bodyFilter3.frequency.value = 1400;
    this.bodyFilter3.Q.value = 4.0;

    this.bodyMixGain = this.ctx.createGain();
    this.bodyMixGain.gain.value = 0.15; // Controlled acoustic resonance blend

    // Tape Flutter & Micro Pitch Drift
    this.tapeFlutterDelay = this.ctx.createDelay(0.1);
    this.tapeFlutterDelay.delayTime.value = 0.01;
    this.tapeFlutterLfo = this.ctx.createOscillator();
    this.tapeFlutterLfo.type = 'sine';
    this.tapeFlutterLfo.frequency.value = 0.8;
    this.tapeFlutterGain = this.ctx.createGain();
    this.tapeFlutterGain.gain.value = 0.0003;
    this.tapeFlutterLfo.connect(this.tapeFlutterGain);
    this.tapeFlutterGain.connect(this.tapeFlutterDelay.delayTime);
    this.tapeFlutterLfo.start();

    // Analog Tube Hysteresis Saturation WaveShaper
    this.saturationShaper = this.ctx.createWaveShaper();
    this.saturationShaper.curve = this.makeAnalogSaturationCurve(0.3) as unknown as Float32Array<ArrayBuffer>;
    this.saturationShaper.oversample = '2x';

    // Global LFO for filter modulation
    this.globalLfo = this.ctx.createOscillator();
    this.globalLfo.type = 'sine';
    this.globalLfo.frequency.value = 2.0;
    this.globalLfoGain = this.ctx.createGain();
    this.globalLfoGain.gain.value = 300.0;
    this.globalLfo.connect(this.globalLfoGain);
    this.globalLfo.start();

    // Highpass DC-Blocking Filter (eliminates subsonic DC pops and kick thumps below 25Hz)
    const masterDcBlocker = this.ctx.createBiquadFilter();
    masterDcBlocker.type = 'highpass';
    masterDcBlocker.frequency.value = 25.0;
    masterDcBlocker.Q.value = 0.707;

    // Signal Routing Chain:
    // Voices -> MasterGain -> MasterDCBlocker -> (Dry bus + FX Sends) -> Limiter -> MasterSoftClipper -> Analyser -> Destination
    this.masterGain.connect(masterDcBlocker);

    // Dry path to limiter
    masterDcBlocker.connect(this.limiter);

    // FX Sends
    masterDcBlocker.connect(this.delayNode);
    masterDcBlocker.connect(this.reverbNode);

    // FX Returns
    this.delayMixGain.connect(this.limiter);
    this.reverbMixGain.connect(this.limiter);

    // Stereo Soundstage Panner Node
    if (this.ctx.createStereoPanner) {
      this.pannerNode = this.ctx.createStereoPanner();
      this.pannerNode.pan.value = 0.0;
    }

    if (this.pannerNode) {
      this.limiter.connect(this.pannerNode);
      this.pannerNode.connect(this.masterSoftClipper);
    } else {
      this.limiter.connect(this.masterSoftClipper);
    }
    this.masterSoftClipper.connect(this.analyser);
    this.analyser.connect(this.ctx.destination);

    this.isInitialized = true;
  }

  private makeMasterSoftClipCurve(): Float32Array {
    const samples = 65536;
    const curve = new Float32Array(samples);
    for (let i = 0; i < samples; i++) {
      const x = (i * 2) / (samples - 1) - 1;
      // Hyperbolic tangent (tanh) soft-clipper:
      // Exact 1:1 linear gain near 0, smooth analog saturation approaching 1.0 without hard clipping
      curve[i] = Math.tanh(x);
    }
    return curve;
  }

  private createSpatialImpulse(duration: number, absorption: number, diffusion: number, luminanceDepth: number): AudioBuffer {
    if (!this.ctx) throw new Error('AudioContext missing');
    const sampleRate = this.ctx.sampleRate;
    const dur = Math.max(0.1, Math.min(6.0, duration));
    const length = Math.floor(sampleRate * dur);
    const impulse = this.ctx.createBuffer(2, length, sampleRate);
    const left = impulse.getChannelData(0);
    const right = impulse.getChannelData(1);

    // Early reflections (first 20-80ms)
    const earlyCount = Math.floor(8 + diffusion * 16);
    const earlyMaxSamples = Math.floor(sampleRate * (0.02 + luminanceDepth * 0.06));

    for (let e = 0; e < earlyCount; e++) {
      const pos = Math.floor((Math.random() * 0.9 + 0.1) * earlyMaxSamples);
      if (pos < length) {
        const amp = (1 - absorption * 0.7) * (1 - pos / earlyMaxSamples);
        left[pos] += (Math.random() * 2 - 1) * amp;
        right[pos] += (Math.random() * 2 - 1) * amp;
      }
    }

    // Diffuse tail decay with absorption damping
    const decayFactor = 1.2 + absorption * 4.0;
    for (let i = 0; i < length; i++) {
      const t = i / length;
      const env = Math.pow(1 - t, decayFactor);
      const lpFilter = Math.exp(-t * (2.5 + absorption * 5.0));
      left[i] += (Math.random() * 2 - 1) * env * lpFilter;
      right[i] += (Math.random() * 2 - 1) * env * lpFilter;
    }

    return impulse;
  }

  private makeAnalogSaturationCurve(warmth: number): Float32Array {
    const samples = 44100;
    const curve = new Float32Array(samples);
    const drive = 1.0 + warmth * 3.5;
    for (let i = 0; i < samples; i++) {
      const x = (i * 2) / samples - 1;
      const asymX = x > 0 ? x : x * (1 - warmth * 0.18);
      curve[i] = Math.tanh(asymX * drive) / Math.tanh(drive);
    }
    return curve;
  }

  private createReverbImpulse(duration: number, decay: number): AudioBuffer {
    if (!this.ctx) throw new Error('AudioContext missing');
    const sampleRate = Math.min(48000, this.ctx.sampleRate || 44100);
    const safeDuration = Math.max(0.1, Math.min(2.0, duration));
    const length = Math.floor(sampleRate * safeDuration);
    const impulse = this.ctx.createBuffer(2, length, sampleRate);
    const left = impulse.getChannelData(0);
    const right = impulse.getChannelData(1);

    for (let i = 0; i < length; i++) {
      const n = i / length;
      const env = Math.pow(1 - n, decay);
      left[i] = (Math.random() * 2 - 1) * env;
      right[i] = (Math.random() * 2 - 1) * env;
    }
    return impulse;
  }

  private makeDistortionCurve(amount: number): Float32Array {
    const k = typeof amount === 'number' && !isNaN(amount) ? amount : 20;
    const n_samples = 44100;
    const curve = new Float32Array(n_samples);
    const deg = Math.PI / 180;
    for (let i = 0; i < n_samples; ++i) {
      const x = (i * 2) / n_samples - 1;
      const raw = ((3 + k) * x * 20 * deg) / (Math.PI + k * Math.abs(x));
      // Normalize to prevent wild clipping spikes
      curve[i] = Math.max(-0.85, Math.min(0.85, raw * 0.45));
    }
    return curve;
  }

  public updateParameters(params: Partial<PatchParameters>): void {
    if (!params) return;
    this.patchParams = {
      ...(this.patchParams || DEFAULT_PATCH_PARAMS),
      ...params,
    };
    if (!this.ctx || !this.isInitialized) return;

    const p = this.patchParams;
    const now = this.ctx.currentTime;

    // Dynamic Gain Staging & Automatic Headroom Compensation
    const resVal = Number.isFinite(p.resonance) ? p.resonance : 1.2;
    const fmVal = Number.isFinite(p.fmAmount) ? p.fmAmount : 0.25;
    const unisonVal = Number.isFinite(p.unisonVoices) ? Math.max(1, p.unisonVoices) : 2;

    const resonanceScaling = 1.0 / (1.0 + Math.pow(Math.max(0, resVal) / 3.5, 1.2) * 0.35);
    const fmScaling = 1.0 / (1.0 + Math.max(0, fmVal) * 0.45);
    const unisonScaling = 1.0 / Math.sqrt(unisonVal);
    const bodyScaling = p.physicalModel
      ? 1.0 / (1.0 + Math.max(0, p.physicalModel.acousticWeight || 0) * 0.5)
      : 1.0;
    const saturationScaling = p.physicalModel
      ? 1.0 / (1.0 + Math.max(0, p.physicalModel.analogSaturationWarmth || 0) * 0.35)
      : 1.0;

    const autoHeadroomFactor = Math.min(
      1.0,
      Math.max(0.40, (resonanceScaling * fmScaling * unisonScaling * bodyScaling * saturationScaling) || 0.7)
    );

    // Master volume (convert dB to linear gain with auto-headroom scaling)
    if (this.masterGain) {
      const volDb = Math.max(-60.0, Math.min(6.0, Number.isFinite(p.masterVolume) ? p.masterVolume : -3.0));
      const userMasterGain = Math.pow(10, volDb / 20);
      const totalMasterGain = Math.max(0.15, (userMasterGain * autoHeadroomFactor * 0.55) || 0.3);
      this.safeSetTargetAtTime(this.masterGain.gain, totalMasterGain, now, 0.02, 0.3);
    }

    // Limiter toggle & threshold
    if (this.limiter) {
      const threshDb = Math.max(-18.0, Math.min(0.0, Number.isFinite(p.threshold) ? p.threshold : -1.0));
      const targetThreshold = p.limiterEnabled ? threshDb : -0.5;
      this.safeSetTargetAtTime(this.limiter.threshold, targetThreshold, now, 0.02, -1.0);
      this.safeSetTargetAtTime(this.limiter.ratio, p.limiterEnabled ? 4.0 : 2.0, now, 0.02, 4.0);
      this.safeSetTargetAtTime(this.limiter.attack, 0.01, now, 0.02, 0.01);
      this.safeSetTargetAtTime(this.limiter.release, 0.15, now, 0.02, 0.15);
    }

    // LFO Rate & Depth
    if (this.globalLfo && this.globalLfoGain) {
      const lfoRateVal = Number.isFinite(p.lfoRate) ? Math.max(0.05, p.lfoRate) : 2.0;
      const lfoDepthVal = Number.isFinite(p.lfoDepth) ? Math.max(0, Math.min(1.0, p.lfoDepth)) : 0.25;
      this.safeSetTargetAtTime(this.globalLfo.frequency, lfoRateVal, now, 0.05, 2.0);
      // LFO Depth maps to filter cutoff modulation range (0 to 3000 Hz)
      const modAmount = lfoDepthVal * 2500;
      this.safeSetTargetAtTime(this.globalLfoGain.gain, modAmount, now, 0.05, 625);

      // Custom Periodic Wave for LFO constructed via DFT of image's spatial line profile
      if (p.customLfoProfile && p.customLfoProfile.length > 0) {
        const profile = p.customLfoProfile;
        const N = profile.length;
        const numHarmonics = Math.min(16, Math.floor(N / 2));
        const real = new Float32Array(numHarmonics);
        const imag = new Float32Array(numHarmonics);
        real[0] = 0;
        imag[0] = 0;
        for (let k = 1; k < numHarmonics; k++) {
          let sumR = 0;
          let sumI = 0;
          for (let n = 0; n < N; n++) {
            const angle = (2 * Math.PI * k * n) / N;
            sumR += profile[n] * Math.cos(angle);
            sumI += profile[n] * Math.sin(angle);
          }
          real[k] = sumR / N;
          imag[k] = sumI / N;
        }
        try {
          const customWave = this.ctx.createPeriodicWave(real, imag, { disableNormalization: false });
          this.globalLfo.setPeriodicWave(customWave);
        } catch {
          this.globalLfo.type = 'sine';
        }
      }
    }

    // FX Mix and 6-Effect Matrix toggles & Parameter Updates
    const fx = p.effects;

    if (this.delayMixGain && this.delayNode && this.delayFeedbackGain) {
      const isDelayOn = fx?.delay ? fx.delay.enabled : true;
      const p1 = Number.isFinite(fx?.delay?.param1) ? fx!.delay!.param1! : (fx?.delay?.intensity ?? 0.5);
      const p2 = Number.isFinite(fx?.delay?.param2) ? fx!.delay!.param2! : (fx?.delay?.intensity ?? 0.5);
      const timeSec = Math.max(0.01, Math.min(2.0, 0.05 + p1 * 0.75)); // 50ms to 800ms
      const fbVal = isDelayOn ? Math.max(0, Math.min(0.95, p2 * 0.85)) : 0.0;
      const mixVal = isDelayOn ? Math.max(0, Math.min(1.0, Math.max(p.delayMix || 0, p1 * 0.5))) : 0.0;

      this.safeSetTargetAtTime(this.delayNode.delayTime, timeSec, now, 0.05, 0.3);
      this.safeSetTargetAtTime(this.delayFeedbackGain.gain, fbVal, now, 0.05, 0.0);
      this.safeSetTargetAtTime(this.delayMixGain.gain, mixVal, now, 0.05, 0.0);
    }

    if (this.reverbMixGain && this.reverbNode) {
      const isReverbOn = fx?.reverb ? fx.reverb.enabled : true;
      const p1 = Number.isFinite(fx?.reverb?.param1) ? fx!.reverb!.param1! : (fx?.reverb?.intensity ?? 0.6);
      const p2 = Number.isFinite(fx?.reverb?.param2) ? fx!.reverb!.param2! : (fx?.reverb?.intensity ?? 0.6);
      const wetVal = isReverbOn ? Math.max(0, Math.min(1.0, Math.max(p.reverbMix || 0, p1 * 0.8))) : 0.0;
      this.safeSetTargetAtTime(this.reverbMixGain.gain, wetVal, now, 0.05, 0.0);

      const targetDecay = Math.max(0.2, Math.min(10.0, 0.5 + p2 * 5.5)); // 0.5s to 6.0s
      if (Math.abs(targetDecay - this.lastReverbDecay) > 0.3) {
        this.lastReverbDecay = targetDecay;
        try {
          this.reverbNode.buffer = this.createReverbImpulse(targetDecay, 2.0);
        } catch {
          // Reverb impulse fallback
        }
      }
    }

    if (this.chorusMixGain) {
      const isChorusOn = fx?.chorus ? fx.chorus.enabled : true;
      const p2 = Number.isFinite(fx?.chorus?.param2) ? fx!.chorus!.param2! : (fx?.chorus?.intensity ?? 0.45);
      const val = isChorusOn ? Math.max(0, Math.min(1.0, p2 * 0.45)) : 0.0;
      this.safeSetTargetAtTime(this.chorusMixGain.gain, val, now, 0.05, 0.0);
    }

    if (this.phaserMixGain) {
      const isPhaserOn = fx?.phaser ? fx.phaser.enabled : true;
      const p2 = Number.isFinite(fx?.phaser?.param2) ? fx!.phaser!.param2! : (fx?.phaser?.intensity ?? 0.5);
      const val = isPhaserOn ? Math.max(0, Math.min(1.0, p2 * 0.45)) : 0.0;
      this.safeSetTargetAtTime(this.phaserMixGain.gain, val, now, 0.05, 0.0);
    }

    if (this.flangerMixGain) {
      const isFlangerOn = fx?.flanger ? fx.flanger.enabled : true;
      const p2 = Number.isFinite(fx?.flanger?.param2) ? fx!.flanger!.param2! : (fx?.flanger?.intensity ?? 0.4);
      const val = isFlangerOn ? Math.max(0, Math.min(1.0, p2 * 0.45)) : 0.0;
      this.safeSetTargetAtTime(this.flangerMixGain.gain, val, now, 0.05, 0.0);
    }

    if (this.distortionMixGain) {
      const isDistortionOn = fx?.distortion ? fx.distortion.enabled : true;
      const p1 = Number.isFinite(fx?.distortion?.param1) ? fx!.distortion!.param1! : (fx?.distortion?.intensity ?? 0.35);
      const val = isDistortionOn ? Math.max(0, Math.min(1.0, p1 * 0.4)) : 0.0;
      this.safeSetTargetAtTime(this.distortionMixGain.gain, val, now, 0.05, 0.0);
    }

    // Physical Modeling & Temporal Era Acoustic Modulation Updates
    const eraVal = Number.isFinite(p.temporalEraVal)
      ? p.temporalEraVal!
      : (p.temporalEra?.eraVal ?? 0.5);

    if (p.physicalModel) {
      const pm = p.physicalModel;

      if (this.bodyFilter1 && this.bodyFilter2 && this.bodyFilter3 && this.bodyMixGain) {
        const freqs = pm.bodyResonanceFreqs || [200, 600, 1800];
        const damping = Number.isFinite(pm.bodyDamping) ? pm.bodyDamping : 0.5;
        const qVal = Math.max(0.5, 12.0 * (1 - damping));
        const acWeight = Number.isFinite(pm.acousticWeight) ? pm.acousticWeight : 0.3;
        this.safeSetTargetAtTime(this.bodyFilter1.frequency, freqs[0], now, 0.05, 200);
        this.safeSetTargetAtTime(this.bodyFilter1.Q, qVal, now, 0.05, 4.0);
        this.safeSetTargetAtTime(this.bodyFilter2.frequency, freqs[1], now, 0.05, 600);
        this.safeSetTargetAtTime(this.bodyFilter2.Q, qVal, now, 0.05, 4.0);
        this.safeSetTargetAtTime(this.bodyFilter3.frequency, freqs[2], now, 0.05, 1800);
        this.safeSetTargetAtTime(this.bodyFilter3.Q, qVal, now, 0.05, 4.0);
        this.safeSetTargetAtTime(this.bodyMixGain.gain, acWeight * 0.25, now, 0.05, 0.0);
      }

      if (this.tapeFlutterLfo && this.tapeFlutterGain) {
        const eraFlutterSpeed = eraVal < 0.38 ? 1.8 + (0.38 - eraVal) * 2.5 : 0.8;
        const eraFlutterDepth = eraVal < 0.38 ? 0.35 + (0.38 - eraVal) * 0.45 : 0.05;
        const speed = pm.tapeFlutterSpeed ?? eraFlutterSpeed;
        const depthMs = (pm.tapeFlutterDepth ?? eraFlutterDepth) * 0.0006;
        this.safeSetTargetAtTime(this.tapeFlutterLfo.frequency, speed, now, 0.05, 0.8);
        this.safeSetTargetAtTime(this.tapeFlutterGain.gain, depthMs, now, 0.05, 0.0001);
      }

      if (this.saturationShaper) {
        const eraWarmth = eraVal < 0.62 ? 0.5 + (0.62 - eraVal) * 0.6 : 0.2;
        this.saturationShaper.curve = this.makeAnalogSaturationCurve(pm.analogSaturationWarmth ?? eraWarmth) as unknown as Float32Array<ArrayBuffer>;
      }

      if (this.reverbNode && pm.spatialReflection) {
        try {
          const sr = pm.spatialReflection;
          this.reverbNode.buffer = this.createSpatialImpulse(
            sr.roomSizeSeconds,
            sr.absorption,
            sr.diffusion,
            sr.luminanceDepth
          );
        } catch {
          // Reverb impulse fallback
        }
      }
    }

    // 1. Spatial Frequency & "Image Focus" Grain (The Optical Depth Engine)
    const focusDepth = Number.isFinite(p.opticalFocusDepth) ? p.opticalFocusDepth! : 0.5;
    if (this.reverbMixGain && focusDepth < 0.38) {
      const washMix = (0.38 - focusDepth) * 0.45;
      const targetWet = Math.min(0.85, (p.reverbMix || 0.3) + washMix);
      this.safeSetTargetAtTime(this.reverbMixGain.gain, targetWet, now, 0.05, 0.3);
    }

    // 2. Symmetry, Geometry & Fractal Grid Density (The Architectural Sequencer)
    const gridSymmetry = Number.isFinite(p.gridSymmetryDensity) ? p.gridSymmetryDensity! : 0.5;
    if (this.globalLfo && this.globalLfoGain) {
      if (gridSymmetry > 0.5) {
        const seqRate = 2.0 * Math.pow(2, Math.floor((gridSymmetry - 0.5) * 4)); // 2Hz, 4Hz, 8Hz, 16Hz
        this.safeSetTargetAtTime(this.globalLfo.frequency, seqRate, now, 0.05, 2.0);
        this.safeSetTargetAtTime(this.globalLfoGain.gain, 250.0 + gridSymmetry * 450.0, now, 0.05, 300);
      } else {
        const droneRate = 0.1 + gridSymmetry * 0.8;
        this.safeSetTargetAtTime(this.globalLfo.frequency, droneRate, now, 0.05, 0.5);
        this.safeSetTargetAtTime(this.globalLfoGain.gain, 140.0, now, 0.05, 140);
      }
    }

    // 3. Lighting Angle & Shadow Azimuth (The Virtual Soundstage Panner)
    const azimuth = Number.isFinite(p.lightAzimuthAngle) ? p.lightAzimuthAngle! : 180;
    const elevation = Number.isFinite(p.lightElevationAngle) ? p.lightElevationAngle! : 45;
    if (this.pannerNode) {
      let panX = Math.cos((azimuth * Math.PI) / 180);
      if (elevation > 65) {
        panX *= 0.25;
      }
      this.safeSetTargetAtTime(this.pannerNode.pan, Math.max(-0.92, Math.min(0.92, panX * 0.75)), now, 0.05, 0.0);
    }

    // 4. Color Contrast Saturation & Complementary Hue Clash (The Harmonic Tension Generator)
    const chromaticClash = Number.isFinite(p.chromaticClash) ? p.chromaticClash! : 0.2;
    if (this.ringModGain && this.ringModOsc) {
      if (chromaticClash > 0.35) {
        const clashFreq = 120 + chromaticClash * 180;
        this.safeSetTargetAtTime(this.ringModOsc.frequency, clashFreq, now, 0.05, 150);
        this.safeSetTargetAtTime(this.ringModGain.gain, (chromaticClash - 0.35) * 0.16, now, 0.05, 0.0);
      } else {
        this.safeSetTargetAtTime(this.ringModGain.gain, 0.0, now, 0.05, 0.0);
      }
    }

    // 5. Semantic Object "Weight" & Density (The Material Physics Matrix)
    const densityWeight = Number.isFinite(p.semanticDensityWeight) ? p.semanticDensityWeight! : 0.5;
    if (this.bodyFilter1 && this.bodyFilter2 && this.bodyFilter3) {
      if (densityWeight > 0.6) {
        this.safeSetTargetAtTime(this.bodyFilter1.frequency, 45, now, 0.05, 45);
        this.safeSetTargetAtTime(this.bodyFilter2.frequency, 95, now, 0.05, 95);
        this.safeSetTargetAtTime(this.bodyFilter3.frequency, 220, now, 0.05, 220);
      } else if (densityWeight < 0.35) {
        this.safeSetTargetAtTime(this.bodyFilter1.frequency, 1150, now, 0.05, 1150);
        this.safeSetTargetAtTime(this.bodyFilter2.frequency, 2400, now, 0.05, 2400);
        this.safeSetTargetAtTime(this.bodyFilter3.frequency, 3600, now, 0.05, 3600);
      }
    }

    // Update ongoing active voice filters
    const safeCutoff = Number.isFinite(p.cutoffOffset) ? Math.max(50, Math.min(20000, p.cutoffOffset)) : 2400;
    const safeRes = Number.isFinite(p.resonance) ? Math.max(0.1, Math.min(10.0, p.resonance)) : 1.2;
    this.activeVoices.forEach((voice) => {
      this.safeSetTargetAtTime(voice.filter.frequency, safeCutoff, now, 0.05, 2400);
      this.safeSetTargetAtTime(voice.filter.Q, safeRes, now, 0.05, 1.2);
    });
  }

  private killVoiceImmediately(voice: PolyVoice, fadeSec = 0.008): void {
    if (!this.ctx) return;
    const now = this.ctx.currentTime;
    try {
      voice.ampGain.gain.cancelScheduledValues(now);
      const currentVal = voice.ampGain.gain.value;
      const safeVal = Number.isFinite(currentVal) ? Math.max(0.0001, currentVal) : 0.0001;
      this.safeSetValueAtTime(voice.ampGain.gain, safeVal, now);
      this.safeLinearRampToValueAtTime(voice.ampGain.gain, 0.0, now + fadeSec);
    } catch {
      // ignore scheduling errors if already inactive
    }

    const cleanupDelayMs = Math.ceil((fadeSec + 0.005) * 1000);
    setTimeout(() => {
      this.cleanupVoiceNodes(voice);
    }, cleanupDelayMs);
  }

  private cleanupVoiceNodes(voice: PolyVoice): void {
    try { voice.osc1.stop(); voice.osc1.disconnect(); } catch {}
    try { voice.osc2.stop(); voice.osc2.disconnect(); } catch {}
    try { if (voice.subOsc) { voice.subOsc.stop(); voice.subOsc.disconnect(); } } catch {}
    try { if (voice.fmModulator) { voice.fmModulator.stop(); voice.fmModulator.disconnect(); } } catch {}
    try { if (voice.fmGain) voice.fmGain.disconnect(); } catch {}
    try { if (voice.hammerClick) { voice.hammerClick.stop(); voice.hammerClick.disconnect(); } } catch {}
    try { if (voice.hammerGain) voice.hammerGain.disconnect(); } catch {}
    try { if (voice.shaperNode) voice.shaperNode.disconnect(); } catch {}
    try { voice.filter.disconnect(); } catch {}
    try { voice.ampGain.disconnect(); } catch {}
  }

  public noteOn(midiNote: number, velocity = 0.8): void {
    // Plugin/DAW mode: notes are rendered by the C++ engine only.
    if (isPluginHostEnvironment()) return;
    if (!this.isInitialized || !this.ctx) {
      this.initAudio().then(() => {
        this.noteOn(midiNote, velocity);
      }).catch(() => {});
      return;
    }

    if (this.ctx.state === 'suspended') {
      this.ctx.resume().catch(() => {});
    }

    // 1. Immediate Voice Retriggering & Fast Voice Stealing (8ms crossfade)
    if (this.activeVoices.has(midiNote)) {
      const activeVoice = this.activeVoices.get(midiNote)!;
      this.killVoiceImmediately(activeVoice, 0.008);
      this.activeVoices.delete(midiNote);
    }

    // Instantly kill any fading release tails for this note so rapid inputs do not smear
    this.fadingVoices.forEach((v) => {
      if (v.note === midiNote) {
        this.killVoiceImmediately(v, 0.008);
        this.fadingVoices.delete(v);
      }
    });

    // Max Polyphony Hard Stealing (steal oldest voice if >= 10 active)
    if (this.activeVoices.size >= 10) {
      let oldestNote: number | null = null;
      let oldestTime = Infinity;
      this.activeVoices.forEach((v, note) => {
        if (v.startTime < oldestTime) {
          oldestTime = v.startTime;
          oldestNote = note;
        }
      });
      if (oldestNote !== null) {
        const oldestVoice = this.activeVoices.get(oldestNote)!;
        this.killVoiceImmediately(oldestVoice, 0.008);
        this.activeVoices.delete(oldestNote);
      }
    }

    // Limit fading release tails to prevent audio choke during fast runs
    if (this.fadingVoices.size >= 8) {
      let oldestFading: PolyVoice | null = null;
      let oldestTime = Infinity;
      this.fadingVoices.forEach((v) => {
        if (v.startTime < oldestTime) {
          oldestTime = v.startTime;
          oldestFading = v;
        }
      });
      if (oldestFading) {
        this.killVoiceImmediately(oldestFading, 0.008);
        this.fadingVoices.delete(oldestFading);
      }
    }

    const freq = 440 * Math.pow(2, (midiNote - 69) / 12);
    const safeFreq = Number.isFinite(freq) && freq > 0 ? freq : 440;
    const params = this.patchParams || DEFAULT_PATCH_PARAMS;
    const now = this.ctx.currentTime;

    // 2. Voice Amp Gain Envelope (Guaranteed smooth floor for zero-latency response)
    const ampGain = this.ctx.createGain();
    this.safeSetValueAtTime(ampGain.gain, 0, now);

    let engineGainBoost = 1.0;
    if (params.engineType === 'acoustic_piano_organ') {
      engineGainBoost = 1.85; // Triangle/sine waves have lower RMS energy
    } else if (params.engineType === 'overdriven_saw_stack' || params.engineType === 'analog_brass') {
      engineGainBoost = 1.35; // Waveshaper compensation
    } else if (params.engineType === 'fm_square_bell' || params.engineType === 'digital_fm_bells') {
      engineGainBoost = 1.25;
    } else {
      engineGainBoost = 1.20;
    }

    const activeVoiceCount = Math.max(1, this.activeVoices.size + 1);
    const polyphonicScale = Math.max(0.70, 1.0 / Math.pow(activeVoiceCount, 0.15));
    const unisonVal = Number.isFinite(params.unisonVoices) ? Math.max(1, params.unisonVoices) : 2;
    const unisonScale = 1.0 / Math.sqrt(unisonVal);
    const vel = Number.isFinite(velocity) ? Math.max(0.05, Math.min(1.0, velocity)) : 0.8;
    const peakAmp = Math.min(0.45, Math.max(0.10, vel * 0.35 * unisonScale * polyphonicScale * engineGainBoost));

    // ADSR Envelope: 12ms smooth de-clicked attack floor prevents waveform step discontinuities
    const microAttackFloor = 0.012;
    const safeAttack = Number.isFinite(params.attackTime) ? Math.max(microAttackFloor, Math.min(5.0, params.attackTime)) : 0.025;
    const safeDecay = Number.isFinite(params.decayTime) ? Math.max(0.02, Math.min(10.0, params.decayTime)) : 0.8;
    const safeSustain = Number.isFinite(params.sustainLevel) ? Math.max(0.35, Math.min(1.0, params.sustainLevel)) : 0.7;

    // Smooth onset ramp starting at precise t=now: zero amplitude at start, ramping to peakAmp
    this.safeSetValueAtTime(ampGain.gain, 0.0, now);
    this.safeLinearRampToValueAtTime(ampGain.gain, peakAmp, now + safeAttack);
    this.safeSetTargetAtTime(
      ampGain.gain,
      peakAmp * safeSustain,
      now + safeAttack,
      safeDecay,
      peakAmp * 0.7
    );

    // 3. Resonant Filter
    const filter = this.ctx.createBiquadFilter();
    filter.type = 'lowpass';
    const safeCutoff = Number.isFinite(params.cutoffOffset) ? Math.max(350, Math.min(20000, params.cutoffOffset)) : 2500;
    const safeRes = Number.isFinite(params.resonance) ? Math.max(0.1, Math.min(10.0, params.resonance)) : 1.0;
    this.safeSetValueAtTime(filter.frequency, safeCutoff, now);
    this.safeSetValueAtTime(filter.Q, safeRes, now);

    // Connect LFO to Filter
    if (this.globalLfoGain) {
      this.globalLfoGain.connect(filter.frequency);
    }

    // 4. Oscillators & Synthesis Engine Topological Swapping
    let osc1: OscillatorNode;
    let osc2: OscillatorNode;
    let subOsc: OscillatorNode | undefined;
    let fmModulator: OscillatorNode | undefined;
    let fmGain: GainNode | undefined;
    let hammerClick: OscillatorNode | undefined;
    let hammerGain: GainNode | undefined;
    let shaperNode: WaveShaperNode | undefined;

    const detuneVal = Number.isFinite(params.detuneCents) ? params.detuneCents : 5.0;

    if (params.engineType === 'acoustic_piano_organ') {
      osc1 = this.ctx.createOscillator();
      osc1.type = 'triangle';
      this.safeSetValueAtTime(osc1.frequency, safeFreq, now);

      osc2 = this.ctx.createOscillator();
      osc2.type = 'sine';
      this.safeSetValueAtTime(osc2.frequency, safeFreq * Math.max(1.0, params.fmRatio || 2.0), now);

      subOsc = this.ctx.createOscillator();
      subOsc.type = 'sine';
      this.safeSetValueAtTime(subOsc.frequency, safeFreq / 2, now);

      const subGain = this.ctx.createGain();
      subGain.gain.value = (Number.isFinite(params.subOscLevel) ? params.subOscLevel : 0.25) * 0.35;
      subOsc.connect(subGain);
      subGain.connect(filter);
      subOsc.start(now);

      osc1.connect(filter);
      osc2.connect(filter);

    } else if (params.engineType === 'overdriven_saw_stack' || params.engineType === 'analog_brass') {
      osc1 = this.ctx.createOscillator();
      osc1.type = 'sawtooth';
      this.safeSetValueAtTime(osc1.frequency, safeFreq, now);
      this.safeSetValueAtTime(osc1.detune, -detuneVal * 1.5, now);

      osc2 = this.ctx.createOscillator();
      osc2.type = 'sawtooth';
      this.safeSetValueAtTime(osc2.frequency, safeFreq, now);
      this.safeSetValueAtTime(osc2.detune, detuneVal * 1.5, now);

      subOsc = this.ctx.createOscillator();
      subOsc.type = 'sawtooth';
      this.safeSetValueAtTime(subOsc.frequency, safeFreq * 1.004, now);

      const subGain = this.ctx.createGain();
      subGain.gain.value = (Number.isFinite(params.subOscLevel) ? params.subOscLevel : 0.25) * 0.35;
      subOsc.connect(subGain);

      // Clean signal path straight to resonant filter for lush analog brass
      osc1.connect(filter);
      osc2.connect(filter);
      subGain.connect(filter);

      subOsc.start(now);

    } else if (params.engineType === 'fm_square_bell' || params.engineType === 'digital_fm_bells') {
      osc1 = this.ctx.createOscillator();
      osc1.type = 'square';
      this.safeSetValueAtTime(osc1.frequency, safeFreq, now);

      osc2 = this.ctx.createOscillator();
      osc2.type = 'square';
      this.safeSetValueAtTime(osc2.frequency, safeFreq * Math.max(1, Math.round(params.fmRatio || 2.0)), now);
      this.safeSetValueAtTime(osc2.detune, detuneVal, now);

      fmModulator = this.ctx.createOscillator();
      fmModulator.type = 'sine';
      this.safeSetValueAtTime(fmModulator.frequency, safeFreq * (params.fmRatio || 3.0), now);

      fmGain = this.ctx.createGain();
      const fmModDepth = safeFreq * (Number.isFinite(params.fmAmount) ? params.fmAmount : 0.3) * 1.5;
      const fmRamp = 0.008;
      this.safeSetValueAtTime(fmGain.gain, 0, now);
      this.safeLinearRampToValueAtTime(fmGain.gain, fmModDepth, now + fmRamp);
      this.safeExponentialRampToValueAtTime(fmGain.gain, 0.001, now + fmRamp + Math.max(0.08, (params.decayTime || 0.5) * 0.8));

      fmModulator.connect(fmGain);
      fmGain.connect(osc1.frequency);
      fmModulator.start(now);

      subOsc = this.ctx.createOscillator();
      subOsc.type = 'square';
      this.safeSetValueAtTime(subOsc.frequency, safeFreq / 2, now);
      const subGain = this.ctx.createGain();
      subGain.gain.value = (Number.isFinite(params.subOscLevel) ? params.subOscLevel : 0.2) * 0.3;
      subOsc.connect(subGain);
      subGain.connect(filter);
      subOsc.start(now);

      osc1.connect(filter);
      osc2.connect(filter);

    } else {
      osc1 = this.ctx.createOscillator();
      osc1.type = 'sawtooth';
      this.safeSetValueAtTime(osc1.frequency, safeFreq, now);

      osc2 = this.ctx.createOscillator();
      osc2.type = 'square';
      this.safeSetValueAtTime(osc2.frequency, safeFreq, now);
      this.safeSetValueAtTime(osc2.detune, detuneVal * 1.5, now);

      subOsc = this.ctx.createOscillator();
      subOsc.type = 'square';
      this.safeSetValueAtTime(subOsc.frequency, safeFreq / 2, now);
      const subGain = this.ctx.createGain();
      subGain.gain.value = (Number.isFinite(params.subOscLevel) ? params.subOscLevel : 0.25) * 0.35;
      subOsc.connect(subGain);
      subGain.connect(filter);
      subOsc.start(now);

      osc1.connect(filter);
      osc2.connect(filter);
    }

    // Connect Filter -> AmpGain -> Master
    filter.connect(ampGain);

    if (this.masterGain) {
      ampGain.connect(this.masterGain);
    }

    osc1.start(now);
    osc2.start(now);

    this.activeVoices.set(midiNote, {
      note: midiNote,
      velocity: vel,
      osc1,
      osc2,
      subOsc,
      fmModulator,
      fmGain,
      hammerClick,
      hammerGain,
      shaperNode,
      filter,
      ampGain,
      startTime: now,
    });
  }

  public noteOff(midiNote: number): void {
    if (!this.ctx || !this.activeVoices.has(midiNote)) return;

    const voice = this.activeVoices.get(midiNote)!;
    this.activeVoices.delete(midiNote);
    this.fadingVoices.add(voice);

    const now = this.ctx.currentTime;
    const p = this.patchParams || DEFAULT_PATCH_PARAMS;
    const release = Number.isFinite(p.releaseTime) ? Math.max(0.02, p.releaseTime) : 0.45;

    try {
      voice.ampGain.gain.cancelScheduledValues(now);
      const currentGain = voice.ampGain.gain.value;
      const safeVal = Number.isFinite(currentGain) ? Math.max(0.0001, currentGain) : 0.0001;
      this.safeSetValueAtTime(voice.ampGain.gain, safeVal, now);
      this.safeLinearRampToValueAtTime(voice.ampGain.gain, 0.00001, now + release);
    } catch {
      // ignore
    }

    const stopDelayMs = Math.ceil((release + 0.05) * 1000);
    setTimeout(() => {
      this.cleanupVoiceNodes(voice);
      this.fadingVoices.delete(voice);
    }, stopDelayMs);
  }

  public panic(): void {
    if (!this.ctx) return;
    this.activeVoices.forEach((voice) => {
      this.killVoiceImmediately(voice, 0.002);
    });
    this.activeVoices.clear();

    this.fadingVoices.forEach((voice) => {
      this.killVoiceImmediately(voice, 0.002);
    });
    this.fadingVoices.clear();
  }

  public getWaveformData(array: Uint8Array): void {
    if (this.analyser) {
      this.analyser.getByteTimeDomainData(array as unknown as Uint8Array<ArrayBuffer>);
    } else {
      array.fill(128);
    }
  }

  public getSpectrumData(array: Uint8Array): void {
    if (this.analyser) {
      this.analyser.getByteFrequencyData(array as unknown as Uint8Array<ArrayBuffer>);
    }
  }
}

export const globalAudioEngine = new WebAudioEngine();
