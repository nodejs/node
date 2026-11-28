// Flags: --expose-gc
// A keep-alive socket tunneled through a proxy must not keep the request
// that created it reachable once it is back in the agent's free pool.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer } from '../common/proxy-server.js';
import fixtures from '../common/fixtures.js';
import { gcUntil } from '../common/gc.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => res.end('ok')));
server.listen(0);
await once(server, 'listening');

const { proxy } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const agent = new https.Agent({
  keepAlive: true,
  proxyEnv: {
    HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  },
  ca: fixtures.readKey('fake-startcom-root-cert.pem'),
});

const ref = new WeakRef(https.get({
  hostname: 'localhost',
  port: server.address().port,
  agent,
}, common.mustCall((res) => res.resume())));

await once(agent, 'free');
await new Promise(setImmediate);
assert.strictEqual(Object.keys(agent.freeSockets).length, 1);
await gcUntil('the first request is collected',
              () => ref.deref() === undefined);
agent.destroy();
proxy.close();
server.close();
