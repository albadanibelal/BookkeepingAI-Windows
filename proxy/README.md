# BookkeepingAI Proxy (Cloudflare Worker)

Secure proxy that keeps your Anthropic API key hidden from the client app.

## Setup (one-time, ~5 minutes)

### 1. Install Wrangler CLI
```bash
npm install -g wrangler
```

### 2. Login to Cloudflare
```bash
wrangler login
```

### 3. Deploy the worker
```bash
cd proxy
wrangler deploy
```

### 4. Add your API key as a secret
```bash
wrangler secret put ANTHROPIC_API_KEY
```
Paste your key when prompted. It's encrypted and never visible in code.

### 5. (Optional) Add an app token for extra security
```bash
wrangler secret put APP_TOKEN
```
Use any random string. Set the same value in the app's `config.ts` as `appToken`.

### 6. Update the app
After deploying, Cloudflare gives you a URL like:
```
https://bookkeepingai-proxy.YOUR_SUBDOMAIN.workers.dev
```
Set this as `proxyURL` in the app's `config.ts`.

## How it works

```
App → Cloudflare Worker (injects API key) → Anthropic API
```

The API key is stored as an encrypted Cloudflare secret. It never appears in:
- The app source code
- Network requests from the app
- GitHub repos
