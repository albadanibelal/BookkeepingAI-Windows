// BookkeepingAI — Cloudflare Worker Proxy
// This worker sits between the app and Anthropic API.
// The API key is stored as a secret in Cloudflare, never exposed to the client.

const ANTHROPIC_BASE = 'https://api.anthropic.com';

// Allowed origins (update with your app's domain if needed)
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

    // Optional: validate app token to prevent unauthorized use
    // Set APP_TOKEN as a Cloudflare secret alongside ANTHROPIC_API_KEY
    if (env.APP_TOKEN) {
      const appToken = request.headers.get('x-app-token');
      if (appToken !== env.APP_TOKEN) {
        return new Response(JSON.stringify({ error: 'Unauthorized' }), {
          status: 401,
          headers: { ...CORS_HEADERS, 'Content-Type': 'application/json' },
        });
      }
    }

    // Build the Anthropic API URL
    const url = new URL(request.url);
    const anthropicURL = ANTHROPIC_BASE + url.pathname;

    // Clone the request headers, inject the API key
    const headers = new Headers(request.headers);
    headers.set('x-api-key', env.ANTHROPIC_API_KEY);
    headers.delete('x-app-token'); // Don't forward app token to Anthropic

    // Forward the request to Anthropic
    const response = await fetch(anthropicURL, {
      method: request.method,
      headers,
      body: request.method !== 'GET' ? request.body : undefined,
    });

    // Return the response with CORS headers
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
