'use strict';

const common = require('../common.js');
const https = require('https');
const crypto = require('crypto');

const bench = common.createBenchmark(main, {
  n: [100, 1000, 10000],
  sessions: [10, 100, 256],
});

function generateSessionId() {
  return crypto.randomBytes(32).toString('hex');
}

function main({ n, sessions }) {
  const agent = new https.Agent({
    maxCachedSessions: sessions,
  });

  // Create dummy session objects
  const sessionData = Buffer.allocUnsafe(1024);

  bench.start();

  for (let i = 0; i < n; i++) {
    // Simulate session caching operations
    const sessionId = `session-${i % sessions}`;

    // Cache session
    agent._cacheSession(sessionId, sessionData);

    // Occasionally evict sessions
    if (i % 10 === 0) {
      agent._evictSession(`session-${Math.floor(Math.random() * sessions)}`);
    }

    // Get session
    agent._getSession(sessionId);
  }

  bench.end(n);
}