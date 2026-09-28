import React, { useEffect, useRef } from 'react';

interface OscilloscopeProps {
  onGetWaveform: (array: Uint8Array) => void;
  onGetSpectrum: (array: Uint8Array) => void;
}

export const Oscilloscope: React.FC<OscilloscopeProps> = ({
  onGetWaveform,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    let animationFrameId: number;
    const waveformArray = new Uint8Array(512);

    const render = () => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext('2d');
      if (!ctx) return;

      const w = canvas.width;
      const h = canvas.height;

      // Dark background
      ctx.fillStyle = '#06090e';
      ctx.fillRect(0, 0, w, h);

      // Subtle horizontal baseline
      ctx.strokeStyle = '#111927';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, h / 2);
      ctx.lineTo(w, h / 2);
      ctx.stroke();

      onGetWaveform(waveformArray);

      // Check if there is active audio signal
      let hasSignal = false;
      for (let i = 0; i < waveformArray.length; i++) {
        if (waveformArray[i] !== 0 && Math.abs(waveformArray[i] - 128) > 3) {
          hasSignal = true;
          break;
        }
      }

      ctx.lineWidth = 2;
      ctx.strokeStyle = hasSignal ? '#38bdf8' : '#0284c7';
      ctx.shadowColor = hasSignal ? '#00f2fe' : '#0284c7';
      ctx.shadowBlur = hasSignal ? 8 : 2;

      ctx.beginPath();
      const sliceWidth = w / waveformArray.length;
      let x = 0;
      const nowMs = performance.now();

      for (let i = 0; i < waveformArray.length; i++) {
        let y: number;
        if (hasSignal) {
          // Actual real-time audio waveform from engine
          const v = (waveformArray[i] - 128) / 128.0;
          y = (h / 2) - (v * h * 0.46);
        } else {
          // Living ambient baseline indicating active engine connection
          const wave = Math.sin((i * 0.04) + (nowMs * 0.003)) * 1.5;
          y = (h / 2) + wave;
        }

        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);

        x += sliceWidth;
      }
      ctx.stroke();
      ctx.shadowBlur = 0;

      animationFrameId = requestAnimationFrame(render);
    };

    render();

    return () => {
      cancelAnimationFrame(animationFrameId);
    };
  }, [onGetWaveform]);

  return (
    <div className="relative w-full h-11 vst-readout vst-bezel rounded-md overflow-hidden shrink-0 shadow-inner">
      <canvas
        ref={canvasRef}
        width={600}
        height={44}
        className="w-full h-full object-cover"
      />
    </div>
  );
};
