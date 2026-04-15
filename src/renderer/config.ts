// All API calls route through the Cloud Run proxy.
// The API key is stored as an encrypted secret in Cloud Run — never in this code.

export const Config = {
  // Cloud Run proxy — all API calls go through here
  proxyURL: 'https://bookkeepingai-proxy-470863874819.us-west1.run.app',

  // License key (persisted in localStorage)
  get licenseKey(): string {
    return localStorage.getItem('bookkeepingai_license_key') || '';
  },
  set licenseKey(key: string) {
    if (key) {
      localStorage.setItem('bookkeepingai_license_key', key);
    } else {
      localStorage.removeItem('bookkeepingai_license_key');
    }
  },

  // Remote URL for the system prompt (legacy single-pass — kept as fallback)
  promptURL: 'https://gist.githubusercontent.com/albadanibelal/bd15666b818f221d97445e010d53339f/raw/bookkeeping-pnl.txt',

  // Bundled fallback prompt file
  fallbackPromptPath: './bookkeeper-pnl.md',

  // Two-pass prompt files (bundled)
  extractPromptPath: './bookkeeper-extract.md',
  classifyPromptPath: './bookkeeper-classify.md',
};

// ---- System Prompt (remote-controlled) ----

let cachedPrompt: string | null = null;

export async function getSystemPrompt(): Promise<string> {
  if (cachedPrompt) return cachedPrompt;

  if (Config.promptURL) {
    try {
      const response = await fetch(Config.promptURL, { cache: 'no-cache' });
      if (response.ok) {
        let text = await response.text();
        text = stripFrontmatter(text);
        cachedPrompt = text;
        return cachedPrompt;
      }
    } catch (err) {
      console.warn('Failed to fetch remote prompt, falling back to bundled:', err);
    }
  }

  try {
    const response = await fetch(Config.fallbackPromptPath);
    if (response.ok) {
      let text = await response.text();
      text = stripFrontmatter(text);
      cachedPrompt = text;
      return cachedPrompt;
    }
  } catch (err) {
    console.warn('Failed to load bundled prompt:', err);
  }

  cachedPrompt = 'You are an expert bookkeeper. Analyze the uploaded financial documents and produce a detailed Profit & Loss report in markdown format. Categorize all items as Revenue, COGS, or Operating Expenses. Format dollar amounts as $X,XXX.XX.';
  return cachedPrompt;
}

export function clearPromptCache() {
  cachedPrompt = null;
  cachedExtractPrompt = null;
  cachedClassifyPrompt = null;
}

// ---- Two-Pass Prompts (bundled only) ----

let cachedExtractPrompt: string | null = null;
let cachedClassifyPrompt: string | null = null;

export async function getExtractPrompt(): Promise<string> {
  if (cachedExtractPrompt) return cachedExtractPrompt;
  try {
    const response = await fetch(Config.extractPromptPath);
    if (response.ok) {
      let text = await response.text();
      text = stripFrontmatter(text);
      cachedExtractPrompt = text;
      return cachedExtractPrompt;
    }
  } catch (err) {
    console.warn('Failed to load extract prompt:', err);
  }
  cachedExtractPrompt = 'You are a financial document data extractor. Read the uploaded documents and output a JSON object with every transaction found. Do not classify or categorize — only extract raw data.';
  return cachedExtractPrompt;
}

export async function getClassifyPrompt(): Promise<string> {
  if (cachedClassifyPrompt) return cachedClassifyPrompt;
  try {
    const response = await fetch(Config.classifyPromptPath);
    if (response.ok) {
      let text = await response.text();
      text = stripFrontmatter(text);
      cachedClassifyPrompt = text;
      return cachedClassifyPrompt;
    }
  } catch (err) {
    console.warn('Failed to load classify prompt:', err);
  }
  cachedClassifyPrompt = 'You are an expert bookkeeper. Classify the provided extracted transaction data into a Profit & Loss report.';
  return cachedClassifyPrompt;
}

function stripFrontmatter(text: string): string {
  const match = text.match(/^---\s*\n[\s\S]*?\n---\s*\n([\s\S]*)$/);
  return match ? match[1].trim() : text.trim();
}
