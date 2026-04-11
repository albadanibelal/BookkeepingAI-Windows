import type {
  AnthropicMessage,
  AnthropicRequest,
  AnthropicResponse,
  FileUploadResponse,
} from '../types';

const BASE_URL = 'https://api.anthropic.com/v1';
const API_VERSION = '2023-06-01';
const MODEL = 'claude-sonnet-4-6';

export class AnthropicService {
  // Send messages to Claude
  async sendMessage(
    messages: AnthropicMessage[],
    systemPrompt: string,
    apiKey: string,
    maxTokens = 8192
  ): Promise<string> {
    const body: AnthropicRequest = {
      model: MODEL,
      max_tokens: maxTokens,
      system: systemPrompt,
      messages,
    };

    const response = await fetch(`${BASE_URL}/messages`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-api-key': apiKey,
        'anthropic-version': API_VERSION,
        'anthropic-beta': 'files-api-2025-04-14,pdfs-2024-09-25',
        'anthropic-dangerous-direct-browser-access': 'true',
      },
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
    // Convert base64 to blob
    const byteChars = atob(base64Data);
    const byteNumbers = new Array(byteChars.length);
    for (let i = 0; i < byteChars.length; i++) {
      byteNumbers[i] = byteChars.charCodeAt(i);
    }
    const byteArray = new Uint8Array(byteNumbers);
    const blob = new Blob([byteArray], { type: mimeType });

    const formData = new FormData();
    formData.append('file', blob, fileName);

    const response = await fetch(`${BASE_URL}/files`, {
      method: 'POST',
      headers: {
        'x-api-key': apiKey,
        'anthropic-version': API_VERSION,
        'anthropic-beta': 'files-api-2025-04-14',
        'anthropic-dangerous-direct-browser-access': 'true',
      },
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
        // Strip "data:image/jpeg;base64," prefix
        resolve(dataUrl.split(',')[1] ?? null);
      };
      img.onerror = () => resolve(null);
      img.src = `data:image/png;base64,${base64Data}`;
    });
  }
}

export const anthropicService = new AnthropicService();
