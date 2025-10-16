// mock-paywall.js
// Simple Express app to simulate a paywall + token "scope" checks
const express = require('express');
const app = express();
const port = 3000;

// In-memory "tokens" for testing only (do NOT use real tokens)
const tokenStore = {
  // token: { scopes: [...], acct, cur }
  "a1-premium-example": { scopes: ["basic", "premium"], acct: "CR9004889", cur: "USD" },
  "a1-basic-example":   { scopes: ["basic"], acct: "VRTC7648305", cur: "USD" }
};

// Helper to validate token and required scope
function validateToken(token, requiredScope) {
  if (!token) return { ok: false, reason: "no_token" };
  const record = tokenStore[token];
  if (!record) return { ok: false, reason: "invalid_token" };
  if (requiredScope && !record.scopes.includes(requiredScope)) {
    return { ok: false, reason: "scope_denied", record };
  }
  return { ok: true, record };
}

// Public homepage
app.get('/', (req, res) => {
  res.send(`
    <h2>Mock BrianTrader Paywall (Local)</h2>
    <p>Available tokens for testing (do NOT use on real sites):</p>
    <ul>
      <li>a1-premium-example (has premium)</li>
      <li>a1-basic-example (no premium)</li>
    </ul>
    <p>To test the dashboard: <code>/dashboard?token=&lt;token&gt;</code></p>
  `);
});

// Dashboard route - requires 'premium' scope
app.get('/dashboard', (req, res) => {
  const token = req.query.token;
  const v = validateToken(token, "premium");
  if (!v.ok) {
    // Simulate redirect that a real site might use when scope denied
    if (v.reason === 'scope_denied') {
      // Redirect back to homepage with a hash error like the site did
      return res.redirect('/#error=scope_denied');
    }
    return res.status(401).send(`Access denied (${v.reason}). Provide a valid token.`);
  }

  const info = v.record;
  res.send(`
    <h3>Trading Signals Dashboard (Mock)</h3>
    <p>Account: ${info.acct} | Currency: ${info.cur}</p>
    <p>Signals: [MOCK DATA] BUY BTC, SELL ETH, ...</p>
    <p><a href="/">Back</a></p>
  `);
});

// Simulate an OAuth authorize endpoint (very simplified)
app.get('/authorize', (req, res) => {
  // Query: ?client_id=...&scope=...&token=...
  const { scope, token } = req.query;
  const tokenRecord = tokenStore[token];
  if (!tokenRecord) {
    return res.redirect('/#error=invalid_token');
  }
  // If requested scope not present, simulate denial
  const requestedScopes = scope ? scope.split(' ') : [];
  const missing = requestedScopes.filter(s => !tokenRecord.scopes.includes(s));
  if (missing.length > 0) {
    return res.redirect('/#error=scope_denied');
  }
  // Otherwise grant access
  return res.redirect('/dashboard?token=' + encodeURIComponent(token));
});

app.listen(port, () => {
  console.log(`Mock paywall app listening at http://localhost:${port}`);
  console.log(`Try: http://localhost:${port}/dashboard?token=a1-premium-example`);
  console.log(`Try: http://localhost:${port}/dashboard?token=a1-basic-example`);
});
