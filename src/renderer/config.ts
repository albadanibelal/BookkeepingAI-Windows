// All API calls route through the Cloudflare Worker proxy.
// The API key is stored as an encrypted secret in Cloudflare — never in this code.

export const Config = {
  // Cloudflare Worker proxy — all API calls go through here
  proxyURL: 'https://it-ub.albadanibelal.workers.dev',

  // Optional app token for extra security (must match APP_TOKEN secret in Cloudflare)
  appToken: '',

  // Remote URL for the system prompt
  promptURL: 'https://gist.githubusercontent.com/albadanibelal/bd15666b818f221d97445e010d53339f/raw/bookkeeping-pnl.txt',

  // Bundled fallback prompt file
  fallbackPromptPath: './bookkeeper-pnl.md',
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
}

function stripFrontmatter(text: string): string {
  const match = text.match(/^---\s*\n[\s\S]*?\n---\s*\n([\s\S]*)$/);
  return match ? match[1].trim() : text.trim();
}
