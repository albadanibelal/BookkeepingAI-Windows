// Electron API exposed via preload
export interface ElectronAPI {
  openFileDialog: () => Promise<Array<{ name: string; path: string; data: string; size: number }>>;
  savePDF: (base64Data: string, defaultName: string) => Promise<boolean>;
}

declare global {
  interface Window {
    electronAPI?: ElectronAPI;
  }
}

// Chat Message
export interface ChatMessage {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: Date;
  isProgress?: boolean;
  pdfData?: string; // base64
}

// Uploaded File
export interface UploadedFile {
  id: string;
  name: string;
  mimeType: string;
  data: string; // base64
  fileId?: string; // Anthropic Files API file_id
  isUploading: boolean;
  uploadError?: string;
}

// P&L Report (for PDF generation)
export interface PnLReport {
  businessName?: string;
  period?: string;
  revenue?: PnLCategory[];
  cogs?: PnLCategory[];
  taxableCOGS?: PnLCategory[];
  nonTaxableCOGS?: PnLCategory[];
  operatingExpenses?: PnLCategory[];
  totalRevenue?: number;
  totalCOGS?: number;
  totalTaxableCOGS?: number;
  totalNonTaxableCOGS?: number;
  totalOpex?: number;
  grossProfit?: number;
  netIncome?: number;
  notes?: string[];
  sourceDocuments?: string[];
}

export interface PnLCategory {
  category: string;
  items?: PnLLineItem[];
}

export interface PnLLineItem {
  description?: string;
  amount?: number;
  date?: string;
}

// Anthropic API Types
export interface AnthropicRequest {
  model: string;
  max_tokens: number;
  system: string;
  messages: AnthropicMessage[];
  temperature?: number;
}

export interface AnthropicMessage {
  role: string;
  content: AnthropicContent[];
}

export interface AnthropicContent {
  type: string;
  text?: string;
  source?: AnthropicSource;
}

export interface AnthropicSource {
  type: string;
  file_id?: string;
  media_type?: string;
  data?: string;
}

export interface AnthropicResponse {
  content?: Array<{ type: string; text?: string }>;
  error?: { message?: string; type?: string };
}

export interface FileUploadResponse {
  id?: string;
  error?: { message?: string; type?: string };
}

// Supported file extensions
export const SUPPORTED_EXTENSIONS = ['pdf', 'png', 'jpg', 'jpeg', 'webp', 'csv', 'xlsx', 'heic'];

export function getMimeType(filename: string): string {
  const ext = filename.split('.').pop()?.toLowerCase() ?? '';
  const map: Record<string, string> = {
    pdf: 'application/pdf',
    png: 'image/png',
    jpg: 'image/jpeg',
    jpeg: 'image/jpeg',
    webp: 'image/webp',
    heic: 'image/heic',
    csv: 'text/csv',
    xlsx: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
  };
  return map[ext] ?? 'application/octet-stream';
}

export function getFileIcon(mimeType: string): string {
  if (mimeType === 'application/pdf') return '📄';
  if (mimeType.startsWith('image/')) return '🖼️';
  if (mimeType.includes('csv') || mimeType.includes('excel') || mimeType.includes('spreadsheet')) return '📊';
  return '📎';
}
