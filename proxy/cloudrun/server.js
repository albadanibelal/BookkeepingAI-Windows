const http = require('http');
const https = require('https');

const PORT = process.env.PORT || 8080;
const ANTHROPIC_API_KEY = process.env.ANTHROPIC_API_KEY;
const APP_TOKEN = process.env.APP_TOKEN || '';

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, anthropic-version, anthropic-beta, x-app-token',
};

const server = http.createServer((req, res) => {
  // CORS preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204, CORS_HEADERS);
    res.end();
    return;
  }

  // App token validation
  if (APP_TOKEN && req.headers['x-app-token'] !== APP_TOKEN) {
    res.writeHead(401, { ...CORS_HEADERS, 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'Unauthorized' }));
    return;
  }

  if (!ANTHROPIC_API_KEY) {
    res.writeHead(500, { ...CORS_HEADERS, 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ error: 'ANTHROPIC_API_KEY not set' }));
    return;
  }

  // Collect request body
  const chunks = [];
  req.on('data', (chunk) => chunks.push(chunk));
  req.on('end', () => {
    const body = Buffer.concat(chunks);

    // Build headers for Anthropic
    const headers = {
      'x-api-key': ANTHROPIC_API_KEY,
      'Content-Type': req.headers['content-type'] || 'application/json',
    };
    if (req.headers['anthropic-version']) headers['anthropic-version'] = req.headers['anthropic-version'];
    if (req.headers['anthropic-beta']) headers['anthropic-beta'] = req.headers['anthropic-beta'];

    const options = {
      hostname: 'api.anthropic.com',
      path: req.url,
      method: req.method,
      headers,
      timeout: 600000, // 10 minutes
    };

    const proxyReq = https.request(options, (proxyRes) => {
      // Forward response with CORS headers
      const responseHeaders = { ...CORS_HEADERS };
      for (const [key, value] of Object.entries(proxyRes.headers)) {
        if (key.toLowerCase() !== 'access-control-allow-origin') {
          responseHeaders[key] = value;
        }
      }

      res.writeHead(proxyRes.statusCode, responseHeaders);
      proxyRes.pipe(res);
    });

    proxyReq.on('error', (err) => {
      res.writeHead(502, { ...CORS_HEADERS, 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: `Proxy error: ${err.message}` }));
    });

    proxyReq.on('timeout', () => {
      proxyReq.destroy();
      res.writeHead(504, { ...CORS_HEADERS, 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: 'Request timed out' }));
    });

    if (body.length > 0) {
      proxyReq.write(body);
    }
    proxyReq.end();
  });
});

server.listen(PORT, () => {
  console.log(`BookkeepingAI proxy running on port ${PORT}`);
});
