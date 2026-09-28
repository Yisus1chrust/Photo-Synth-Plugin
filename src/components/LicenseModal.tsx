import React, { useState, useEffect } from 'react';
import { X, KeyRound, Check, Clipboard, Copy, ExternalLink, Loader2 } from 'lucide-react';
import {
  LicenseData,
  GUMROAD_STORE_URL,
  verifyGumroadLicense,
  deactivateLicense,
} from '../services/licenseService';

interface LicenseModalProps {
  isOpen: boolean;
  onClose: () => void;
  currentLicense: LicenseData | null;
  onLicenseChanged: (license: LicenseData | null) => void;
}

export const LicenseModal: React.FC<LicenseModalProps> = ({
  isOpen,
  onClose,
  currentLicense,
  onLicenseChanged,
}) => {
  const [licenseKey, setLicenseKey] = useState('');
  const [email, setEmail] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);

  const isActive = currentLicense?.status === 'active';

  useEffect(() => {
    if (currentLicense) {
      setLicenseKey(currentLicense.key);
      setEmail(currentLicense.email || '');
    } else {
      setLicenseKey('');
      setEmail('');
    }
    setErrorMsg(null);
  }, [currentLicense, isOpen]);

  // Keep modal open if unlicensed
  if (!isOpen && isActive) return null;
  if (!isOpen && !isActive) return null;

  const handleActivate = async (e?: React.FormEvent) => {
    e?.preventDefault();
    if (!licenseKey.trim()) {
      setErrorMsg('Please enter your license key.');
      return;
    }

    setIsLoading(true);
    setErrorMsg(null);

    const res = await verifyGumroadLicense(licenseKey, email);
    setIsLoading(false);

    if (res.success && res.data) {
      onLicenseChanged(res.data);
    } else {
      setErrorMsg(res.message || 'Invalid license key.');
    }
  };

  const handleDeactivate = () => {
    deactivateLicense();
    onLicenseChanged(null);
    setLicenseKey('');
    setEmail('');
    setErrorMsg(null);
  };

  const handlePaste = async () => {
    try {
      const text = await navigator.clipboard.readText();
      if (text) setLicenseKey(text.trim());
    } catch {
      // ignore
    }
  };

  const handleCopy = async () => {
    if (!licenseKey) return;
    try {
      await navigator.clipboard.writeText(licenseKey);
      setCopied(true);
      setTimeout(() => setCopied(false), 1500);
    } catch {
      // ignore
    }
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-sm select-none font-mono">
      {/* Clean Modal Card matching reference screenshot with synth styling */}
      <div className="w-full max-w-md bg-[#11141c] border border-[#273042] rounded-xl shadow-2xl p-5 sm:p-6 text-zinc-100 space-y-4 relative">
        {/* Header */}
        <div className="flex items-center justify-between">
          <h2 className="text-base font-bold text-zinc-100 flex items-center gap-2">
            <KeyRound className="w-4 h-4 text-cyan-400" />
            <span>License Details</span>
          </h2>
          {isActive && (
            <button
              onClick={onClose}
              className="text-zinc-400 hover:text-zinc-100 p-1 transition-colors"
              title="Close"
            >
              <X className="w-4 h-4" />
            </button>
          )}
        </div>

        {/* Status Line (Reference screenshot) */}
        <div className="flex items-center gap-3 text-xs text-zinc-400 pb-1">
          <div className="flex items-center gap-1.5">
            <span>Status:</span>
            <span
              className={`px-2 py-0.5 rounded text-[11px] font-bold ${
                isActive
                  ? 'bg-emerald-950 text-emerald-400 border border-emerald-500/40'
                  : 'bg-zinc-800 text-zinc-400 border border-zinc-700'
              }`}
            >
              {isActive ? '✓ Active' : '× Inactive'}
            </span>
          </div>
          <div>Expires: <span className="text-zinc-300">Never</span></div>
          <div className="text-zinc-500">No limit</div>
        </div>

        {/* Simple Form */}
        <form onSubmit={handleActivate} className="space-y-3 pt-1">
          {/* Email */}
          <div>
            <label className="block text-[11px] text-zinc-400 mb-1">
              Your Email
            </label>
            <input
              type="email"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              disabled={isActive}
              placeholder="you@example.com"
              className="w-full bg-[#0a0d13] border border-[#222838] focus:border-cyan-500 focus:outline-none rounded-lg px-3 py-2 text-xs text-zinc-100 placeholder-zinc-600 disabled:opacity-60"
            />
          </div>

          {/* License Key */}
          <div>
            <div className="flex items-center justify-between mb-1">
              <label className="text-[11px] text-zinc-400">
                License Key
              </label>
              {!isActive && (
                <a
                  href={GUMROAD_STORE_URL}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-[11px] text-cyan-400 hover:text-cyan-300 flex items-center gap-1"
                >
                  <span>Get License</span>
                  <ExternalLink className="w-2.5 h-2.5" />
                </a>
              )}
            </div>

            <div className="relative flex items-center">
              <input
                type="text"
                value={licenseKey}
                onChange={(e) => setLicenseKey(e.target.value)}
                disabled={isActive}
                placeholder="12345678-1234-1234-1234-123456789012"
                className="w-full bg-[#0a0d13] border border-[#222838] focus:border-cyan-500 focus:outline-none rounded-lg pl-3 pr-8 py-2 text-xs text-zinc-100 placeholder-zinc-600 font-mono tracking-wider disabled:opacity-60"
              />
              <button
                type="button"
                onClick={isActive ? handleCopy : handlePaste}
                title={isActive ? 'Copy' : 'Paste'}
                className="absolute right-2 text-zinc-400 hover:text-cyan-300 p-1"
              >
                {isActive ? (
                  copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />
                ) : (
                  <Clipboard className="w-3.5 h-3.5" />
                )}
              </button>
            </div>
          </div>

          {/* Error Message */}
          {errorMsg && (
            <p className="text-[11px] text-rose-400 pt-0.5">{errorMsg}</p>
          )}

          {/* Action Buttons */}
          <div className="pt-2 flex items-center justify-end gap-2">
            {isActive ? (
              <>
                <button
                  type="button"
                  onClick={handleDeactivate}
                  className="text-xs text-zinc-400 hover:text-rose-400 px-3 py-1.5 rounded transition-colors"
                >
                  Deactivate
                </button>
                <button
                  type="button"
                  onClick={onClose}
                  className="bg-cyan-500/20 hover:bg-cyan-500/30 border border-cyan-500/50 text-cyan-300 text-xs px-4 py-1.5 rounded font-bold transition-all"
                >
                  Done
                </button>
              </>
            ) : (
              <button
                type="submit"
                disabled={isLoading || !licenseKey.trim()}
                className="w-full bg-cyan-600 hover:bg-cyan-500 disabled:opacity-50 text-black text-xs font-bold py-2 rounded-lg transition-all flex items-center justify-center gap-1.5 mt-2 cursor-pointer disabled:cursor-not-allowed"
              >
                {isLoading ? (
                  <>
                    <Loader2 className="w-3.5 h-3.5 animate-spin" />
                    <span>Verifying...</span>
                  </>
                ) : (
                  <span>Activate</span>
                )}
              </button>
            )}
          </div>
        </form>
      </div>
    </div>
  );
};
