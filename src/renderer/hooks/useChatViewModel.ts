import { useState, useCallback, useRef, useEffect } from 'react';
import type { ChatMessage, UploadedFile, AnthropicMessage, AnthropicContent } from '../types';
import { getMimeType } from '../types';
import { Config, getSystemPromptWithVendors, mergeVendorMappings } from '../config';
import { anthropicService } from '../services/anthropicService';
import { generatePDF } from '../services/pdfGenerator';
import type { PnLReport } from '../types';

// Simple UUID generator (no external dep needed)
function genId(): string {
  return Math.random().toString(36).substring(2) + Date.now().toString(36);
}

export function useChatViewModel() {
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [inputText, setInputText] = useState('');
  const [uploadedFiles, setUploadedFiles] = useState<UploadedFile[]>([]);
  const [isSending, setIsSending] = useState(false);
  const [showSettings, setShowSettings] = useState(false);

  const conversationHistory = useRef<AnthropicMessage[]>([]);
  const chatSession = useRef(0);

  // API key is handled by Cloudflare proxy — app just needs a placeholder
  const effectiveAPIKey = 'proxy-managed';

  const hasFilesUploading = uploadedFiles.some((f) => f.isUploading);


  // Initialize with greeting
  const onAppear = useCallback(() => {
    const greeting = `Hello! I'm **BookkeepingAI**, your dedicated Profit & Loss report agent.

Just upload your financial documents using the area below (PDFs, photos of receipts, bank statements, invoices, CSV files — anything works) and I'll automatically extract every transaction, categorize it correctly, and produce a clean, professional P&L statement with a downloadable PDF report.

You can also type any bookkeeping question in the message box below.

*Note: I'm specialized for bookkeeping only. I won't be able to help with unrelated topics.*`;

    setMessages([
      {
        id: genId(),
        role: 'assistant',
        content: greeting,
        timestamp: new Date(),
      },
    ]);

  }, []);

  // Add files
  const addFiles = useCallback(
    async (fileInfos: Array<{ name: string; data: string }>) => {
      const session = chatSession.current;
      for (const fileInfo of fileInfos) {
        const ext = fileInfo.name.split('.').pop()?.toLowerCase() ?? '';
        const mimeType = getMimeType(fileInfo.name);

        const file: UploadedFile = {
          id: genId(),
          name: fileInfo.name,
          mimeType,
          data: fileInfo.data,
          isUploading: true,
        };

        setUploadedFiles((prev) => [...prev, file]);

        // Upload to Files API (works via proxy in browser, directly in Electron)
        (async () => {
          try {
            let uploadData = file.data;
            let uploadMime = file.mimeType;

            if (file.mimeType.startsWith('image/')) {
              const compressed = await anthropicService.compressImage(file.data);
              if (compressed) {
                uploadData = compressed;
                uploadMime = 'image/jpeg';
              }
            }

            const fileId = await anthropicService.uploadFile(
              uploadData,
              file.name,
              uploadMime,
              effectiveAPIKey
            );

            if (chatSession.current !== session) return;
            setUploadedFiles((prev) =>
              prev.map((f) =>
                f.id === file.id ? { ...f, fileId, isUploading: false } : f
              )
            );
          } catch (error: any) {
            if (chatSession.current !== session) return;
            // Fall back to base64 inline (no fileId, will use base64 in fileToContent)
            setUploadedFiles((prev) =>
              prev.map((f) =>
                f.id === file.id ? { ...f, isUploading: false } : f
              )
            );
          }
        })();
      }
    },
    [effectiveAPIKey]
  );

  const removeFile = useCallback((index: number) => {
    setUploadedFiles((prev) => prev.filter((_, i) => i !== index));
  }, []);

  // Auto-send: when files are attached and all uploads complete, automatically process them
  const autoSendTriggered = useRef(false);

  useEffect(() => {
    if (
      uploadedFiles.length > 0 &&
      !uploadedFiles.some((f) => f.isUploading) &&
      !isSending &&
      !autoSendTriggered.current
    ) {
      autoSendTriggered.current = true;
      // Small delay to batch multiple file drops
      const timer = setTimeout(() => {
        sendMessageWithFiles(uploadedFiles);
      }, 500);
      return () => clearTimeout(timer);
    }
    if (uploadedFiles.length === 0) {
      autoSendTriggered.current = false;
    }
  }, [uploadedFiles, isSending]);

  // File to API content
  const fileToContent = useCallback((file: UploadedFile): AnthropicContent[] => {
    if (file.mimeType.startsWith('text/') || file.mimeType.includes('csv')) {
      const text = atob(file.data);
      return [{ type: 'text', text: `File: ${file.name}\n\n${text}` }];
    }

    if (file.fileId) {
      const type = file.mimeType === 'application/pdf' ? 'document' : 'image';
      return [{ type, source: { type: 'file', file_id: file.fileId } }];
    }

    // Fallback: inline base64
    if (file.mimeType.startsWith('image/')) {
      return [
        {
          type: 'image',
          source: { type: 'base64', media_type: 'image/jpeg', data: file.data },
        },
      ];
    }

    if (file.mimeType === 'application/pdf') {
      return [
        {
          type: 'document',
          source: { type: 'base64', media_type: 'application/pdf', data: file.data },
        },
      ];
    }

    return [{ type: 'text', text: `[${file.name} — unsupported format. Please export as PDF or CSV.]` }];
  }, []);

  // Check if response is a P&L report
  const looksLikePnLReport = (text: string): boolean => {
    const lower = text.toLowerCase();
    const hasHeader =
      lower.includes('profit & loss') ||
      lower.includes('profit and loss') ||
      lower.includes('p&l');
    const hasTotals =
      lower.includes('total revenue') ||
      lower.includes('net income') ||
      lower.includes('gross profit');
    return hasHeader && hasTotals;
  };

  // Generate PDF from report
  const handleGeneratePDF = useCallback(
    async (markdown: string, sourceFiles: string[]) => {
      const session = chatSession.current;
      const progressId = genId();
      setMessages((prev) => [
        ...prev,
        {
          id: progressId,
          role: 'assistant',
          content: 'Generating your PDF report...',
          timestamp: new Date(),
          isProgress: true,
        },
      ]);

      const structurePrompt = `Convert the following Profit & Loss report into a JSON object. Return ONLY valid JSON, no markdown fences, no explanation.

IMPORTANT: Keep descriptions SHORT (max 40 chars each). Combine similar small items. This must fit in one response.
IMPORTANT: Split COGS into taxableCOGS, nonTaxableCOGS, and mixedCOGS arrays per CDTFA rules. Also include totalTaxableCOGS, totalNonTaxableCOGS, and totalMixedCOGS subtotals. The "cogs" field should contain ALL COGS items (taxable, non-taxable, mixed, and lottery combined).

Required JSON format:
{"businessName":"string","businessAddress":"string or null","businessPhone":"string or null","businessType":"string or null","period":"string","revenue":[{"category":"string","items":[{"description":"string","amount":0.00,"date":"string"}]}],"taxableCOGS":[{"category":"string","items":[{"description":"string","amount":0.00,"date":"string"}]}],"nonTaxableCOGS":[{"category":"string","items":[{"description":"string","amount":0.00,"date":"string"}]}],"mixedCOGS":[{"category":"string","items":[{"description":"string","amount":0.00,"date":"string"}]}],"cogs":[{"category":"string","items":[{"description":"string","amount":0.00,"date":"string"}]}],"operatingExpenses":[{"category":"string","items":[{"description":"string","amount":0.00,"date":"string"}]}],"totalRevenue":0.00,"totalTaxableCOGS":0.00,"totalNonTaxableCOGS":0.00,"totalMixedCOGS":0.00,"totalCOGS":0.00,"totalOpex":0.00,"grossProfit":0.00,"netIncome":0.00,"notes":["string"],"sourceDocuments":["string"]}

P&L Report:
${markdown}

Source files: ${sourceFiles.join(', ')}`;

      let attempt = 0;
      const maxAttempts = 3;

      while (attempt < maxAttempts) {
        attempt++;
        try {
          const jsonReply = await anthropicService.sendMessage(
            [{ role: 'user', content: [{ type: 'text', text: structurePrompt }] }],
            'You are a JSON converter. Return ONLY valid JSON. No markdown. No explanation. Keep descriptions under 40 characters.',
            effectiveAPIKey,
            8192
          );

          if (chatSession.current !== session) return;

          let clean = jsonReply
            .replace(/```json/gi, '')
            .replace(/```/g, '')
            .trim();

          const startIdx = clean.indexOf('{');
          const endIdx = clean.lastIndexOf('}');
          if (startIdx !== -1 && endIdx !== -1) {
            clean = clean.substring(startIdx, endIdx + 1);
          }

          // Fix common JSON truncation issues
          // Balance unclosed brackets/braces
          let openBraces = 0, openBrackets = 0;
          let inString = false, escape = false;
          for (const ch of clean) {
            if (escape) { escape = false; continue; }
            if (ch === '\\') { escape = true; continue; }
            if (ch === '"') { inString = !inString; continue; }
            if (inString) continue;
            if (ch === '{') openBraces++;
            if (ch === '}') openBraces--;
            if (ch === '[') openBrackets++;
            if (ch === ']') openBrackets--;
          }
          // Close any unclosed structures
          for (let i = 0; i < openBrackets; i++) clean += ']';
          for (let i = 0; i < openBraces; i++) clean += '}';

          const report: PnLReport = JSON.parse(clean);

          // Post-generation math validation
          const warnings: string[] = [];
          const taxable = report.totalTaxableCOGS ?? 0;
          const nonTaxable = report.totalNonTaxableCOGS ?? 0;
          const mixed = report.totalMixedCOGS ?? 0;
          const totalCOGS = report.totalCOGS ?? 0;
          const grossProfit = report.grossProfit ?? 0;
          const totalRevenue = report.totalRevenue ?? 0;
          const totalOpex = report.totalOpex ?? 0;
          const netIncome = report.netIncome ?? 0;

          const cogsSum = taxable + nonTaxable + mixed;
          if (Math.abs(cogsSum - totalCOGS) > 0.02 && cogsSum > 0) {
            warnings.push(`COGS mismatch: Taxable ($${taxable.toFixed(2)}) + Non-Taxable ($${nonTaxable.toFixed(2)}) + Mixed ($${mixed.toFixed(2)}) = $${cogsSum.toFixed(2)}, but Total COGS = $${totalCOGS.toFixed(2)}`);
          }
          const expectedGross = totalRevenue - totalCOGS;
          if (Math.abs(expectedGross - grossProfit) > 0.02 && totalRevenue > 0) {
            warnings.push(`Gross Profit mismatch: Revenue ($${totalRevenue.toFixed(2)}) - COGS ($${totalCOGS.toFixed(2)}) = $${expectedGross.toFixed(2)}, but Gross Profit = $${grossProfit.toFixed(2)}`);
          }
          const expectedNet = grossProfit - totalOpex;
          if (Math.abs(expectedNet - netIncome) > 0.02 && grossProfit > 0) {
            warnings.push(`Net Income mismatch: Gross Profit ($${grossProfit.toFixed(2)}) - OpEx ($${totalOpex.toFixed(2)}) = $${expectedNet.toFixed(2)}, but Net Income = $${netIncome.toFixed(2)}`);
          }

          if (warnings.length > 0) {
            const existingNotes = report.notes ?? [];
            report.notes = [...existingNotes, '⚠ MATH VALIDATION:', ...warnings];
          }

          const pdfBase64 = generatePDF(report);

          if (pdfBase64) {
            setMessages((prev) => {
              const filtered = prev.filter((m) => m.id !== progressId);
              return [
                ...filtered,
                {
                  id: genId(),
                  role: 'assistant',
                  content: 'Your P&L report PDF is ready! Click "Save PDF" below to download it.',
                  timestamp: new Date(),
                  pdfData: pdfBase64,
                },
              ];
            });
          } else {
            setMessages((prev) =>
              prev.map((m) =>
                m.id === progressId
                  ? { ...m, content: 'PDF generation failed. You can copy the report above manually.', isProgress: false }
                  : m
              )
            );
          }
          return;
        } catch (error: any) {
          if (chatSession.current !== session) return;
          if (error.message?.toLowerCase().includes('rate limit') && attempt < maxAttempts) {
            setMessages((prev) =>
              prev.map((m) =>
                m.id === progressId
                  ? { ...m, content: `Rate limit reached — retrying PDF in 65s (attempt ${attempt} of ${maxAttempts - 1})...` }
                  : m
              )
            );
            await new Promise((r) => setTimeout(r, 65000));
            if (chatSession.current !== session) return;
            continue;
          }

          setMessages((prev) =>
            prev.map((m) =>
              m.id === progressId
                ? { ...m, content: `PDF generation failed: ${error.message}. You can copy the report above manually.`, isProgress: false }
                : m
            )
          );
          return;
        }
      }
    },
    [effectiveAPIKey]
  );

  // Core function to process files and send to API
  const sendMessageWithFiles = useCallback(async (files: UploadedFile[], userText = '') => {
    const session = chatSession.current;

    setIsSending(true);
    setUploadedFiles([]);
    autoSendTriggered.current = false;

    // Fetch the latest system prompt (from remote URL or bundled file)
    const systemPrompt = await getSystemPromptWithVendors();

    // Display user message
    const names = files.map((f) => f.name).join(', ');
    const displayText = (userText ? userText + '\n\n' : '') + `${files.length} file(s): ${names}`;
    setMessages((prev) => [
      ...prev,
      { id: genId(), role: 'user', content: displayText, timestamp: new Date() },
    ]);

    // Show progress while analyzing
    const analyzeProgressId = genId();
    setMessages((prev) => [
      ...prev,
      {
        id: analyzeProgressId,
        role: 'assistant',
        content: `Analyzing ${files.length} document(s)... this may take a minute.`,
        timestamp: new Date(),
        isProgress: true,
      },
    ]);

    try {
      const defaultPrompt = files.length === 1
        ? 'Extract all financial data from this document and generate a complete Profit & Loss report. Split COGS into Taxable and Non-Taxable per CDTFA rules. Include Total Revenue, Taxable COGS subtotal, Non-Taxable COGS subtotal, Total COGS, Gross Profit, Total Operating Expenses, and Net Income. Output the full report in a single response.'
        : `Extract all financial data from these ${files.length} documents and generate a complete Profit & Loss report. Split COGS into Taxable and Non-Taxable per CDTFA rules. Include Total Revenue, Taxable COGS subtotal, Non-Taxable COGS subtotal, Total COGS, Gross Profit, Total Operating Expenses, and Net Income. Output the full report in a single response.`;
      const prompt = userText || defaultPrompt;

      // Build content array with all files
      const allContent: AnthropicContent[] = [];
      for (const file of files) {
        allContent.push(...fileToContent(file));
      }
      allContent.push({ type: 'text', text: prompt });

      // Send as a fresh message (no conversation history) for reproducible reports
      const reply = await anthropicService.sendMessage(
        [{ role: 'user', content: allContent }],
        systemPrompt,
        effectiveAPIKey
      );

      if (chatSession.current !== session) return;

      conversationHistory.current.push({ role: 'user', content: allContent });
      conversationHistory.current.push({
        role: 'assistant',
        content: [{ type: 'text', text: reply }],
      });

      setMessages((prev) => {
        const filtered = prev.filter((m) => m.id !== analyzeProgressId);
        return [
          ...filtered,
          { id: genId(), role: 'assistant', content: reply, timestamp: new Date() },
        ];
      });

      // Auto-learn vendor mappings from the report
      try {
        const vendorMatch = reply.match(/```json\s*\n(\[[\s\S]*?\])\s*\n```/);
        if (vendorMatch) {
          const vendorData = JSON.parse(vendorMatch[1]);
          if (Array.isArray(vendorData) && vendorData.length > 0 && vendorData[0].vendor) {
            mergeVendorMappings(vendorData);
          }
        }
      } catch { /* vendor extraction failed silently — not critical */ }

      // Always attempt PDF generation after document processing
      handleGeneratePDF(reply, files.map((f) => f.name));
    } catch (error: any) {
      if (chatSession.current !== session) return;
      setMessages((prev) => [
        ...prev.filter((m) => m.id !== analyzeProgressId),
        {
          id: genId(),
          role: 'assistant',
          content: `Something went wrong: ${error.message}\n\nPlease try again.`,
          timestamp: new Date(),
        },
      ]);
    }

    if (chatSession.current !== session) return;
    setIsSending(false);
  }, [effectiveAPIKey, fileToContent, handleGeneratePDF]);

  // Send message (text only or text + files via Send button)
  const sendMessage = useCallback(async () => {
    const text = inputText.trim();
    if (!text && uploadedFiles.length === 0) return;

    if (uploadedFiles.length > 0) {
      // If there are files, use the file processing path
      setInputText('');
      await sendMessageWithFiles([...uploadedFiles], text);
      return;
    }

    // Text only
    const session = chatSession.current;
    setIsSending(true);
    setInputText('');

    const systemPrompt = await getSystemPromptWithVendors();

    if (chatSession.current !== session) return;

    setMessages((prev) => [
      ...prev,
      { id: genId(), role: 'user', content: text, timestamp: new Date() },
    ]);

    try {
      const userContent: AnthropicContent[] = [{ type: 'text', text }];
      conversationHistory.current.push({ role: 'user', content: userContent });

      const reply = await anthropicService.sendMessage(
        conversationHistory.current,
        systemPrompt,
        effectiveAPIKey
      );

      if (chatSession.current !== session) return;

      conversationHistory.current.push({
        role: 'assistant',
        content: [{ type: 'text', text: reply }],
      });
      setMessages((prev) => [
        ...prev,
        { id: genId(), role: 'assistant', content: reply, timestamp: new Date() },
      ]);
    } catch (error: any) {
      if (chatSession.current !== session) return;
      setMessages((prev) => [
        ...prev,
        {
          id: genId(),
          role: 'assistant',
          content: `Something went wrong: ${error.message}\n\nPlease try again.`,
          timestamp: new Date(),
        },
      ]);
    }

    if (chatSession.current !== session) return;
    setIsSending(false);
  }, [inputText, uploadedFiles, effectiveAPIKey, sendMessageWithFiles]);

  // Save PDF
  const savePDF = useCallback(async (base64Data: string) => {
    const dateStr = new Date().toLocaleDateString('en-US').replace(/\//g, '-');
    const defaultName = `PnL_Report_${dateStr}.pdf`;

    if (window.electronAPI) {
      await window.electronAPI.savePDF(base64Data, defaultName);
    } else {
      // Browser fallback
      const link = document.createElement('a');
      link.href = `data:application/pdf;base64,${base64Data}`;
      link.download = defaultName;
      link.click();
    }
  }, []);

  // New chat
  const newChat = useCallback(() => {
    chatSession.current += 1;
    conversationHistory.current = [];
    autoSendTriggered.current = false;
    setUploadedFiles([]);
    setInputText('');
    setIsSending(false);
    onAppear();
  }, [onAppear]);

  return {
    messages,
    inputText,
    setInputText,
    uploadedFiles,
    isSending,
    showSettings,
    setShowSettings,
    hasFilesUploading,
    onAppear,
    addFiles,
    removeFile,
    sendMessage,
    savePDF,
    newChat,
  };
}
