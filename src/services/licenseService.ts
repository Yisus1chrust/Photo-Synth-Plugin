export interface LicenseData {
  key: string;
  email: string;
  status: 'active' | 'inactive';
  productName: string;
  productId: string;
  activatedAt?: string;
  uses?: number;
}

export interface VerifyResult {
  success: boolean;
  message?: string;
  data?: LicenseData;
}

const STORAGE_KEY = 'photosynth_gumroad_license';
export const GUMROAD_PRODUCT_ID = 'i9Y9Xewz9kRV9rcS_iBhdA==';
export const GUMROAD_PERMALINK = 'Photo-Synth';
export const GUMROAD_STORE_URL = 'https://modularsoundtech.gumroad.com/l/Photo-Synth';

export function getStoredLicense(): LicenseData | null {
  if (typeof window === 'undefined') return null;
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return null;
    return JSON.parse(raw);
  } catch (err) {
    console.error('Failed to parse stored license:', err);
    return null;
  }
}

export function saveStoredLicense(license: LicenseData | null): void {
  if (typeof window === 'undefined') return;
  try {
    if (license) {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(license));
    } else {
      localStorage.removeItem(STORAGE_KEY);
    }
  } catch (err) {
    console.error('Failed to save stored license:', err);
  }
}

export async function verifyGumroadLicense(
  licenseKey: string,
  email?: string
): Promise<VerifyResult> {
  const cleanKey = licenseKey.trim();
  if (!cleanKey) {
    return { success: false, message: 'Please enter a valid license key.' };
  }

  const cleanEmail = email?.trim() || '';

  // Creator / Developer Sample Keys (from Gumroad Receipt Preview or dev testing)
  const DEV_SAMPLE_KEYS = [
    '6F0E4C97-B72A4E69-A11BF6C4-AF6517E7',
    'TEST-PHOTO-SYNTH-2026',
    'MODULAR-SOUND-DEV',
  ];

  if (DEV_SAMPLE_KEYS.includes(cleanKey.toUpperCase())) {
    const verifiedLicense: LicenseData = {
      key: cleanKey,
      email: cleanEmail || 'creator@modularsoundtech.com',
      status: 'active',
      productName: 'Photo-Synth (Developer / Creator License)',
      productId: GUMROAD_PRODUCT_ID,
      activatedAt: new Date().toISOString(),
      uses: 1,
    };
    saveStoredLicense(verifiedLicense);
    return {
      success: true,
      message: 'License activated successfully! (Creator / Sample Key)',
      data: verifiedLicense,
    };
  }

  const params = new URLSearchParams();
  params.append('product_id', GUMROAD_PRODUCT_ID);
  params.append('license_key', cleanKey);
  params.append('increment_uses_count', 'true');

  let responseData: any = null;

  // Try direct Gumroad verification endpoint first
  try {
    const directRes = await fetch('https://api.gumroad.com/v2/licenses/verify', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: params.toString(),
    });
    responseData = await directRes.json();
  } catch (directErr) {
    // If blocked by browser CORS, try Vite proxy
    try {
      const proxyRes = await fetch('/api/gumroad/v2/licenses/verify', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: params.toString(),
      });
      responseData = await proxyRes.json();
    } catch (proxyErr) {
      return {
        success: false,
        message: 'Could not connect to Gumroad license verification server. Check your internet connection.',
      };
    }
  }

  if (responseData && responseData.success) {
    const purchase = responseData.purchase || {};
    const purchaseEmail = purchase.email || cleanEmail;

    // Optional email check if user supplied an email
    if (cleanEmail && purchase.email && cleanEmail.toLowerCase() !== purchase.email.toLowerCase()) {
      return {
        success: false,
        message: `License key does not match email "${cleanEmail}". Registered email begins with ${purchase.email.slice(0, 3)}***.`,
      };
    }

    const verifiedLicense: LicenseData = {
      key: cleanKey,
      email: purchaseEmail,
      status: 'active',
      productName: purchase.product_name || 'Photo-Synth',
      productId: GUMROAD_PRODUCT_ID,
      activatedAt: new Date().toISOString(),
      uses: responseData.uses || 1,
    };

    saveStoredLicense(verifiedLicense);
    return {
      success: true,
      message: 'License activated successfully!',
      data: verifiedLicense,
    };
  }

  const errorMsg =
    responseData?.message ||
    'Invalid license key. Please check your key or purchase a license on Gumroad.';

  return {
    success: false,
    message: errorMsg,
  };
}

export function deactivateLicense(): void {
  saveStoredLicense(null);
}
