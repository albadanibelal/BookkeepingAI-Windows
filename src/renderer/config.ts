// Remote configuration — update once, all customer apps pick up changes automatically.
//
// Setup:
//   1. Create a secret Gist with your API key (just the key, nothing else)
//   2. Click "Raw", remove the commit hash from the URL
//   3. Paste as apiKeyURL below
//   4. Do the same for your prompt (already done)

export const Config = {
  // Cloudflare Worker proxy URL — set this after deploying the worker
  // Example: 'https://bookkeepingai-proxy.YOUR_SUBDOMAIN.workers.dev'
  // When set, ALL API calls go through the proxy (API key stays on server)
  proxyURL: '',

  // Optional app token for extra security (must match APP_TOKEN secret in Cloudflare)
  appToken: '',

  // Fallback: direct API key (only used if proxyURL is empty)
  fallbackAPIKey: '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',
  apiKeyURL: 'https://gist.githubusercontent.com/albadanibelal/b47db1064817d3c2736a60f6dd99e9d4/raw/api-key.txt',

  // Remote URL for the system prompt
  promptURL: 'https://gist.githubusercontent.com/albadanibelal/bd15666b818f221d97445e010d53339f/raw/bookkeeping-pnl.txt',

  // Bundled fallback prompt file
  fallbackPromptPath: './bookkeeper-pnl.md',
};

// ---- API Key (remote-controlled) ----

let cachedAPIKey: string | null = null;

export async function getAPIKey(): Promise<string> {
  if (cachedAPIKey) return cachedAPIKey;

  // Try remote URL first
  if (Config.apiKeyURL) {
    try {
      const response = await fetch(Config.apiKeyURL, { cache: 'no-cache' });
      if (response.ok) {
        const key = (await response.text()).trim();
        if (key.startsWith('sk-')) {
          cachedAPIKey = key;
          console.log('API key loaded from remote URL');
          return cachedAPIKey;
        }
      }
    } catch (err) {
      console.warn('Failed to fetch remote API key, using fallback:', err);
    }
  }

  // Fall back to hardcoded key
  cachedAPIKey = Config.fallbackAPIKey;
  return cachedAPIKey;
}

export function clearAPIKeyCache() {
  cachedAPIKey = null;
}

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
        console.log('System prompt loaded from remote URL');
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
      console.log('System prompt loaded from bundled file');
      return cachedPrompt;
    }
  } catch (err) {
    console.warn('Failed to load bundled prompt:', err);
  }

  cachedPrompt = 'You are an expert bookkeeper. Analyze the uploaded financial documents and produce a detailed Profit & Loss report in markdown format. Categorize all items as Revenue, COGS, or Operating Expenses. Format dollar amounts as $X,XXX.XX.';
  console.warn('Using minimal fallback prompt');
  return cachedPrompt;
}

export function clearPromptCache() {
  cachedPrompt = null;
}

function stripFrontmatter(text: string): string {
  const match = text.match(/^---\s*\n[\s\S]*?\n---\s*\n([\s\S]*)$/);
  return match ? match[1].trim() : text.trim();
}
