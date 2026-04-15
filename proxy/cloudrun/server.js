const http = require('http');
const https = require('https');
const crypto = require('crypto');
const { Firestore } = require('@google-cloud/firestore');

const PORT = process.env.PORT || 8080;
const ANTHROPIC_API_KEY = process.env.ANTHROPIC_API_KEY;
const ADMIN_SECRET = process.env.ADMIN_SECRET || '';

// Initialize Firestore (auto-authenticates on Cloud Run via default service account)
const db = new Firestore();
const keysCollection = db.collection('licenseKeys');
const usageCollection = db.collection('usageLogs');

// In-memory cache for license keys (5-minute TTL)
const keyCache = new Map();
const CACHE_TTL = 5 * 60 * 1000;

function getCachedKey(key) {
  const entry = keyCache.get(key);
  if (!entry) return null;
  if (Date.now() - entry.timestamp > CACHE_TTL) {
    keyCache.delete(key);
    return null;
  }
  return entry.data;
}

function setCachedKey(key, data) {
  keyCache.set(key, { data, timestamp: Date.now() });
}

// Generate a license key like BK-XXXX-XXXX-XXXX
function generateKey() {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; // no 0/O/1/I confusion
  const segment = () => {
    let s = '';
    for (let i = 0; i < 4; i++) {
      s += chars[crypto.randomInt(chars.length)];
    }
    return s;
  };
  return `BK-${segment()}-${segment()}-${segment()}`;
}

// Parse JSON body from request
function parseBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    req.on('data', (c) => chunks.push(c));
    req.on('end', () => {
      const raw = Buffer.concat(chunks);
      if (raw.length === 0) return resolve({});
      try { resolve(JSON.parse(raw.toString())); }
      catch (e) { reject(new Error('Invalid JSON')); }
    });
    req.on('error', reject);
  });
}

// Send JSON response
function jsonResponse(res, status, data) {
  res.writeHead(status, { ...CORS_HEADERS, 'Content-Type': 'application/json' });
  res.end(JSON.stringify(data));
}

// Check admin auth
function isAdmin(req) {
  const auth = req.headers['authorization'] || '';
  return ADMIN_SECRET && auth === `Bearer ${ADMIN_SECRET}`;
}

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, anthropic-version, anthropic-beta, x-license-key, Authorization',
};

// Validate a license key against Firestore
async function validateKey(key) {
  if (!key) return { valid: false, error: 'No license key provided' };

  // Check cache first
  const cached = getCachedKey(key);
  if (cached) return cached;

  try {
    const doc = await keysCollection.doc(key).get();
    if (!doc.exists) {
      const result = { valid: false, error: 'Invalid license key' };
      setCachedKey(key, result);
      return result;
    }

    const data = doc.data();
    if (!data.isActive) {
      const result = { valid: false, error: 'License key has been revoked' };
      setCachedKey(key, result);
      return result;
    }

    if (data.expiresAt && data.expiresAt.toDate() < new Date()) {
      const result = { valid: false, error: 'License key has expired' };
      setCachedKey(key, result);
      return result;
    }

    const result = { valid: true, plan: data.plan, clientName: data.clientName };
    setCachedKey(key, result);
    return result;
  } catch (err) {
    console.error('Firestore error:', err.message);
    return { valid: false, error: 'Validation service unavailable' };
  }
}

// Log usage asynchronously (fire-and-forget)
function logUsage(licenseKey, endpoint, documentCount) {
  usageCollection.add({
    licenseKey,
    timestamp: Firestore.Timestamp.now(),
    endpoint,
    documentCount: documentCount || 0,
  }).catch((err) => console.error('Usage log error:', err.message));
}

// Count file references in request body
function countDocuments(body) {
  try {
    const data = JSON.parse(body.toString());
    if (!data.messages) return 0;
    let count = 0;
    for (const msg of data.messages) {
      if (Array.isArray(msg.content)) {
        for (const block of msg.content) {
          if (block.type === 'image' || block.type === 'document') count++;
          if (block.source && block.source.file_id) count++;
        }
      }
    }
    return count;
  } catch {
    return 0;
  }
}

const server = http.createServer(async (req, res) => {
  // CORS preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204, CORS_HEADERS);
    res.end();
    return;
  }

  const url = new URL(req.url, `http://${req.headers.host}`);
  const path = url.pathname;

  // ---- Admin: Generate Key ----
  if (path === '/admin/generate-key' && req.method === 'POST') {
    if (!isAdmin(req)) return jsonResponse(res, 401, { error: 'Unauthorized' });
    try {
      const body = await parseBody(req);
      const key = generateKey();
      await keysCollection.doc(key).set({
        clientName: body.clientName || 'Unknown',
        email: body.email || '',
        plan: body.plan || 'standard',
        isActive: true,
        createdAt: Firestore.Timestamp.now(),
        expiresAt: body.expiresAt ? Firestore.Timestamp.fromDate(new Date(body.expiresAt)) : null,
      });
      return jsonResponse(res, 200, { key, clientName: body.clientName });
    } catch (err) {
      return jsonResponse(res, 500, { error: err.message });
    }
  }

  // ---- Admin: List Keys ----
  if (path === '/admin/keys' && req.method === 'GET') {
    if (!isAdmin(req)) return jsonResponse(res, 401, { error: 'Unauthorized' });
    try {
      const active = url.searchParams.get('active');
      let query = keysCollection.orderBy('createdAt', 'desc');
      if (active === 'true') query = query.where('isActive', '==', true);
      if (active === 'false') query = query.where('isActive', '==', false);
      const snapshot = await query.get();
      const keys = [];
      snapshot.forEach((doc) => {
        const data = doc.data();
        keys.push({
          key: doc.id,
          clientName: data.clientName,
          email: data.email,
          plan: data.plan,
          isActive: data.isActive,
          createdAt: data.createdAt ? data.createdAt.toDate().toISOString() : null,
          expiresAt: data.expiresAt ? data.expiresAt.toDate().toISOString() : null,
        });
      });
      return jsonResponse(res, 200, { keys });
    } catch (err) {
      return jsonResponse(res, 500, { error: err.message });
    }
  }

  // ---- Admin: Usage Logs ----
  if (path === '/admin/usage' && req.method === 'GET') {
    if (!isAdmin(req)) return jsonResponse(res, 401, { error: 'Unauthorized' });
    try {
      const keyFilter = url.searchParams.get('key');
      const from = url.searchParams.get('from');
      const to = url.searchParams.get('to');
      const limitParam = parseInt(url.searchParams.get('limit') || '100', 10);

      let query = usageCollection.orderBy('timestamp', 'desc');
      if (keyFilter) query = query.where('licenseKey', '==', keyFilter);
      if (from) query = query.where('timestamp', '>=', Firestore.Timestamp.fromDate(new Date(from)));
      if (to) query = query.where('timestamp', '<=', Firestore.Timestamp.fromDate(new Date(to)));
      query = query.limit(limitParam);

      const snapshot = await query.get();
      const logs = [];
      snapshot.forEach((doc) => {
        const data = doc.data();
        logs.push({
          licenseKey: data.licenseKey,
          timestamp: data.timestamp ? data.timestamp.toDate().toISOString() : null,
          endpoint: data.endpoint,
          documentCount: data.documentCount,
        });
      });

      // Summary stats
      const summary = {};
      logs.forEach((log) => {
        if (!summary[log.licenseKey]) {
          summary[log.licenseKey] = { requests: 0, documents: 0 };
        }
        summary[log.licenseKey].requests++;
        summary[log.licenseKey].documents += log.documentCount || 0;
      });

      return jsonResponse(res, 200, { logs, summary });
    } catch (err) {
      return jsonResponse(res, 500, { error: err.message });
    }
  }

  // ---- Admin: Revoke Key ----
  if (path === '/admin/revoke-key' && req.method === 'POST') {
    if (!isAdmin(req)) return jsonResponse(res, 401, { error: 'Unauthorized' });
    try {
      const body = await parseBody(req);
      if (!body.key) return jsonResponse(res, 400, { error: 'Missing key' });
      await keysCollection.doc(body.key).update({ isActive: false });
      keyCache.delete(body.key); // clear cache immediately
      return jsonResponse(res, 200, { revoked: true, key: body.key });
    } catch (err) {
      return jsonResponse(res, 500, { error: err.message });
    }
  }

  // ---- Public: Validate Key ----
  if (path === '/license/validate' && req.method === 'POST') {
    try {
      const body = await parseBody(req);
      const result = await validateKey(body.key);
      return jsonResponse(res, result.valid ? 200 : 401, result);
    } catch (err) {
      return jsonResponse(res, 500, { error: err.message });
    }
  }

  // ---- Proxy: Forward to Anthropic API ----
  // License key validation for all /v1/* requests
  const licenseKey = req.headers['x-license-key'];
  const keyResult = await validateKey(licenseKey);
  if (!keyResult.valid) {
    return jsonResponse(res, 401, { error: keyResult.error || 'Invalid license key' });
  }

  if (!ANTHROPIC_API_KEY) {
    return jsonResponse(res, 500, { error: 'ANTHROPIC_API_KEY not set' });
  }

  // Collect request body
  const chunks = [];
  req.on('data', (chunk) => chunks.push(chunk));
  req.on('end', () => {
    const body = Buffer.concat(chunks);

    // Log usage (fire-and-forget)
    const docCount = countDocuments(body);
    logUsage(licenseKey, path, docCount);

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
      timeout: 600000,
    };

    const proxyReq = https.request(options, (proxyRes) => {
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
