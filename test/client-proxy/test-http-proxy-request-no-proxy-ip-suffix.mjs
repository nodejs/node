// This tests that plain NO_PROXY entries do not suffix-match IP addresses.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

// Start a server that should NOT be reached directly.
const server = http.createServer(common.mustNotCall());
server.listen(0, '127.0.0.1');
await once(server, 'listening');

// Start a proxy server that should be used for all requests below.
const proxy = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('proxied');
}));
proxy.listen(0);
await once(proxy, 'listening');

{
  // A plain entry must not bypass an IP host by matching a string suffix.
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://127.0.0.1:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: '0.1',
  });

  // The request should go through the proxy (not bypass it).
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /proxied/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

{
  // An IP entry must not bypass a domain host that merely ends with it.
  // foo.127.0.0.1 is not bypassed by the entry 127.0.0.1, so the request
  // takes the proxy path, which rejects the numeric-TLD hostname when it
  // builds the request URL. A bypass regression would instead attempt a
  // direct connection and call the lookup.
  const agent = new http.Agent({
    proxyEnv: {
      HTTP_PROXY: `http://127.0.0.1:${proxy.address().port}`,
      NO_PROXY: '127.0.0.1',
    },
  });
  assert.throws(() => {
    http.request({
      agent,
      hostname: 'foo.127.0.0.1',
      lookup: common.mustNotCall(),
      path: '/',
    });
  }, { code: 'ERR_INVALID_URL' });
  agent.destroy();
}

proxy.close();
server.close();
