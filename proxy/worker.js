// BookkeepingAI — Cloudflare Worker Proxy
// Simple pass-through proxy. API key injected server-side.

const ANTHROPIC_BASE = 'https://api.anthropic.com';

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, anthropic-version, anthropic-beta, x-app-token',
};

export default {
  async fetch(request, env) {
    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: CORS_HEADERS });
    }

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
      return new Response(JSON.stringify({ error: 'ANTHROPIC_API_KEY not set' }), {
        status: 500,
        headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
      });
    }

    const url = new URL(request.url);
    const anthropicURL = ANTHROPIC_BASE + url.pathname;

    const headers = new Headers(request.headers);
    headers.set('x-api-key', env.ANTHROPIC_API_KEY);
    headers.delete('x-app-token');
    headers.delete('host');
    headers.delete('origin');
    headers.delete('referer');

    // Stream the response body through to keep connection alive
    const response = await fetch(anthropicURL, {
      method: request.method,
      headers,
      body: request.method !== 'GET' ? request.body : undefined,
    });

    const responseHeaders = new Headers(response.headers);
    for (const [key, value] of Object.entries(CORS_HEADERS)) {
      responseHeaders.set(key, value);
    }

    // Pass through the response body as a stream
    return new Response(response.body, {
      status: response.status,
      headers: responseHeaders,
    });
  },
};
