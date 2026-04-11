// The prompt is fetched from a remote URL so you can update it once
// and all customer apps pick up the change automatically.
// To use: create a GitHub Gist with your prompt, click "Raw", and paste the URL below.
//
// How to set up:
//   1. Go to https://gist.github.com
//   2. Create a new secret gist with the contents of prompt/bookkeeper-pnl.md
//   3. Click "Raw" on the gist
//   4. Copy the URL (remove the commit hash to always get latest):
//      https://gist.githubusercontent.com/YOUR_USERNAME/GIST_ID/raw/bookkeeper-pnl.md
//   5. Paste it as PROMPT_URL below

export const Config = {
  anthropicAPIKey: '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',

  // Remote URL for the system prompt (set this to your Gist raw URL)
  // Leave empty to use the bundled fallback prompt
  promptURL: 'https://gist.githubusercontent.com/albadanibelal/bd15666b818f221d97445e010d53339f/raw/bookkeeping-pnl.txt',

  // Bundled fallback prompt file path (relative to app root, loaded via fetch)
  fallbackPromptPath: './bookkeeper-pnl.md',
};

// Fetches the system prompt from remote URL, falls back to bundled file
let cachedPrompt: string | null = null;

export async function getSystemPrompt(): Promise<string> {
  if (cachedPrompt) return cachedPrompt;

  // Try remote URL first
  if (Config.promptURL) {
    try {
      const response = await fetch(Config.promptURL, { cache: 'no-cache' });
      if (response.ok) {
        let text = await response.text();
        // Strip YAML frontmatter if present (--- ... ---)
        text = stripFrontmatter(text);
        cachedPrompt = text;
        console.log('System prompt loaded from remote URL');
        return cachedPrompt;
      }
    } catch (err) {
      console.warn('Failed to fetch remote prompt, falling back to bundled:', err);
    }
  }

  // Fall back to bundled prompt file
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

  // Last resort: minimal inline prompt
  cachedPrompt = 'You are an expert bookkeeper. Analyze the uploaded financial documents and produce a detailed Profit & Loss report in markdown format. Categorize all items as Revenue, COGS, or Operating Expenses. Format dollar amounts as $X,XXX.XX.';
  console.warn('Using minimal fallback prompt');
  return cachedPrompt;
}

// Force re-fetch on next call (e.g., after settings change)
export function clearPromptCache() {
  cachedPrompt = null;
}

function stripFrontmatter(text: string): string {
  // Remove YAML frontmatter (--- ... ---) from skill files
  const match = text.match(/^---\s*\n[\s\S]*?\n---\s*\n([\s\S]*)$/);
  return match ? match[1].trim() : text.trim();
}
