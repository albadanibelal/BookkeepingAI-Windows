import type {
  AnthropicMessage,
  AnthropicRequest,
  AnthropicResponse,
  FileUploadResponse,
} from '../types';
import { Config } from '../config';

const ANTHROPIC_DIRECT = 'https://api.anthropic.com/v1';
const VITE_PROXY = '/api/anthropic/v1';
const API_VERSION = '2023-06-01';
const MODEL = 'claude-sonnet-4-6';

// Cache device info (loaded once from Electron main process)
let cachedDeviceInfo: string | null = null;

async function getDeviceHeader(): Promise<string> {
  if (cachedDeviceInfo) return cachedDeviceInfo;
  try {
    if (window.electronAPI?.getDeviceInfo) {
      const info = await window.electronAPI.getDeviceInfo();
      cachedDeviceInfo = JSON.stringify(info);
      return cachedDeviceInfo;
    }
  } catch { /* ignore */ }
  cachedDeviceInfo = '{}';
  return cachedDeviceInfo;
}

function getBaseURL(): string {
  // If Cloudflare proxy is configured, use it
  if (Config.proxyURL) {
    return Config.proxyURL.replace(/\/$/, '') + '/v1';
  }
  // In Electron, call Anthropic directly
  const isElectron = typeof window !== 'undefined' && !!(window as any).electronAPI;
  if (isElectron) return ANTHROPIC_DIRECT;
  // In browser dev, use Vite proxy
  return VITE_PROXY;
}

async function getHeaders(apiKey: string): Promise<Record<string, string>> {
  const headers: Record<string, string> = {
    'Content-Type': 'application/json',
    'anthropic-version': API_VERSION,
    'anthropic-beta': 'files-api-2025-04-14,pdfs-2024-09-25',
  };

  if (Config.proxyURL) {
    // Using proxy — send license key, proxy injects API key
    if (Config.licenseKey) {
      headers['x-license-key'] = Config.licenseKey;
    }
    // Send device info for analytics
    const deviceInfo = await getDeviceHeader();
    if (deviceInfo && deviceInfo !== '{}') {
      headers['x-device-info'] = deviceInfo;
    }
  } else {
    // Direct mode — send API key
    headers['x-api-key'] = apiKey;
    headers['anthropic-dangerous-direct-browser-access'] = 'true';
  }

  return headers;
}

async function getUploadHeaders(apiKey: string): Promise<Record<string, string>> {
  const headers: Record<string, string> = {
    'anthropic-version': API_VERSION,
    'anthropic-beta': 'files-api-2025-04-14',
  };

  if (Config.proxyURL) {
    if (Config.licenseKey) {
      headers['x-license-key'] = Config.licenseKey;
    }
    const deviceInfo = await getDeviceHeader();
    if (deviceInfo && deviceInfo !== '{}') {
      headers['x-device-info'] = deviceInfo;
    }
  } else {
    headers['x-api-key'] = apiKey;
    headers['anthropic-dangerous-direct-browser-access'] = 'true';
  }

  return headers;
}

export class AnthropicService {
  // Send messages to Claude
  async sendMessage(
    messages: AnthropicMessage[],
    systemPrompt: string,
    apiKey: string,
    maxTokens = 16384
  ): Promise<string> {
    const body: AnthropicRequest = {
      model: MODEL,
      max_tokens: maxTokens,
      system: systemPrompt,
      messages,
      temperature: 0,
    };

    const response = await fetch(`${getBaseURL()}/messages`, {
      method: 'POST',
      headers: await getHeaders(apiKey),
      body: JSON.stringify(body),
    });

    const data: AnthropicResponse = await response.json();

    if (data.error) {
      throw new Error(data.error.message ?? `API error (status ${response.status})`);
    }

    if (!data.content) {
      throw new Error('No content in API response');
    }

    return data.content
      .filter((c) => c.type === 'text')
      .map((c) => c.text ?? '')
      .join('\n');
  }

  // Upload file to Anthropic Files API
  async uploadFile(
    base64Data: string,
    fileName: string,
    mimeType: string,
    apiKey: string
  ): Promise<string> {
    const byteChars = atob(base64Data);
    const byteNumbers = new Array(byteChars.length);
    for (let i = 0; i < byteChars.length; i++) {
      byteNumbers[i] = byteChars.charCodeAt(i);
    }
    const byteArray = new Uint8Array(byteNumbers);
    const blob = new Blob([byteArray], { type: mimeType });

    const formData = new FormData();
    formData.append('file', blob, fileName);

    const response = await fetch(`${getBaseURL()}/files`, {
      method: 'POST',
      headers: await getUploadHeaders(apiKey),
      body: formData,
    });

    const data: FileUploadResponse = await response.json();

    if (data.error) {
      throw new Error(data.error.message ?? `Upload failed (status ${response.status})`);
    }

    if (!data.id) {
      throw new Error('No file ID returned from upload');
    }

    return data.id;
  }

  // Validate a license key against the proxy
  async validateLicenseKey(key: string): Promise<{ valid: boolean; plan?: string; clientName?: string; error?: string }> {
    try {
      const response = await fetch(`${Config.proxyURL}/license/validate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ key }),
      });
      return await response.json();
    } catch {
      return { valid: false, error: 'Unable to connect. Check your internet connection.' };
    }
  }

  // Compress image by drawing to canvas
  async compressImage(
    base64Data: string,
    maxDimension = 1568,
    quality = 0.82
  ): Promise<string | null> {
    return new Promise((resolve) => {
      const img = new Image();
      img.onload = () => {
        let w = img.width;
        let h = img.height;

        if (w > maxDimension || h > maxDimension) {
          const ratio = Math.min(maxDimension / w, maxDimension / h);
          w = Math.round(w * ratio);
          h = Math.round(h * ratio);
        }

        const canvas = document.createElement('canvas');
        canvas.width = w;
        canvas.height = h;
        const ctx = canvas.getContext('2d');
        if (!ctx) {
          resolve(null);
          return;
        }
        ctx.drawImage(img, 0, 0, w, h);
        const dataUrl = canvas.toDataURL('image/jpeg', quality);
        resolve(dataUrl.split(',')[1] ?? null);
      };
      img.onerror = () => resolve(null);
      img.src = `data:image/png;base64,${base64Data}`;
    });
  }
}

export const anthropicService = new AnthropicService();
