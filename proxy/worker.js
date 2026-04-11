// BookkeepingAI — Cloudflare Worker Proxy
// Streams responses from Anthropic to avoid Cloudflare's 100s timeout.
// The API key is stored as a secret in Cloudflare, never exposed to the client.

const ANTHROPIC_BASE = 'https://api.anthropic.com';

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, anthropic-version, anthropic-beta, x-app-token',
};

export default {
  async fetch(request, env) {
    // Handle CORS preflight
    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: CORS_HEADERS });
    }

    // Optional app token validation
    if (env.APP_TOKEN) {
      const appToken = request.headers.get('x-app-token');
      if (appToken !== env.APP_TOKEN) {
        return new Response(JSON.stringify({ error: 'Unauthorized' }), {
          status: 401,
          headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
        });
      }
    }

    if (!env.ANTHROPIC_API_KEY) {
      return new Response(JSON.stringify({ error: 'ANTHROPIC_API_KEY secret is not set' }), {
        status: 500,
        headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
      });
    }

    const url = new URL(request.url);
    const anthropicURL = ANTHROPIC_BASE + url.pathname;

    // Clone headers, inject API key
    const headers = new Headers(request.headers);
    headers.set('x-api-key', env.ANTHROPIC_API_KEY);
    headers.delete('x-app-token');
    headers.delete('host');
    headers.delete('origin');
    headers.delete('referer');

    // For message requests, force streaming to avoid Cloudflare timeout
    let body = request.body;
    if (url.pathname.includes('/messages') && request.method === 'POST') {
      try {
        const json = await request.json();
        // Enable streaming
        json.stream = true;
        body = JSON.stringify(json);
        headers.set('Content-Type', 'application/json');
      } catch (e) {
        // If we can't parse, forward as-is
      }
    }

    const response = await fetch(anthropicURL, {
      method: request.method,
      headers,
      body: request.method !== 'GET' ? body : undefined,
    });

    // For streamed responses, collect the full response and return as non-streamed JSON
    // This way the app doesn't need to handle SSE parsing
    if (response.headers.get('content-type')?.includes('text/event-stream')) {
      const reader = response.body.getReader();
      const decoder = new TextDecoder();
      let fullText = '';
      let responseData = null;

      while (true) {
        const { done, value } = await reader.read();
        if (done) break;

        const chunk = decoder.decode(value, { stream: true });
        const lines = chunk.split('\n');

        for (const line of lines) {
          if (!line.startsWith('data: ')) continue;
          const data = line.slice(6).trim();
          if (data === '[DONE]') continue;

          try {
            const event = JSON.parse(data);

            if (event.type === 'message_start') {
              responseData = event.message;
              responseData.content = [];
            } else if (event.type === 'content_block_start') {
              responseData.content.push(event.content_block);
            } else if (event.type === 'content_block_delta') {
              const idx = event.index;
              if (responseData.content[idx] && event.delta.text) {
                responseData.content[idx].text = (responseData.content[idx].text || '') + event.delta.text;
              }
            } else if (event.type === 'message_delta') {
              if (event.delta.stop_reason) {
                responseData.stop_reason = event.delta.stop_reason;
              }
              if (event.usage) {
                responseData.usage = { ...responseData.usage, ...event.usage };
              }
            }
          } catch (e) {
            // Skip unparseable events
          }
        }
      }

      if (responseData) {
        return new Response(JSON.stringify(responseData), {
          status: 200,
          headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
        });
      }
    }

    // Non-streamed responses (file uploads, errors) — forward directly
    const responseHeaders = new Headers(response.headers);
    for (const [key, value] of Object.entries(CORS_HEADERS)) {
      responseHeaders.set(key, value);
    }

    return new Response(response.body, {
      status: response.status,
      headers: responseHeaders,
    });
  },
};
