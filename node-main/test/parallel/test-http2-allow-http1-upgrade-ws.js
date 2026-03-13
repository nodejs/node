// Flags: --expose-internals
'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto) common.skip('missing crypto');

const http2 = require('http2');

const undici = require('internal/deps/undici/undici');
const WebSocketServer = require('../common/websocket-server');

(async function main() {
  const server = http2.createSecureServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    allowHTTP1: true,
  });

  server.on('request', common.mustNotCall());
  new WebSocketServer({ server }); // Handles websocket 'upgrade' events

  await new Promise((resolve) => server.listen(0, resolve));

  await new Promise((resolve, reject) => {
    const ws = new WebSocket(`wss://localhost:${server.address().port}`, {
      dispatcher: new undici.EnvHttpProxyAgent({
        connect: { rejectUnauthorized: false }
      })
    });
    ws.addEventListener('open', common.mustCall(() => {
      ws.close();
      resolve();
    }));
    ws.addEventListener('error', reject);
  });

  server.close();
})().then(common.mustCall());
