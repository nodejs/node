// This tests that origin-form paths are preserved when proxied, including
// paths starting with //.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer } from '../common/proxy-server.js';

const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}, 1));
server.on('error', common.mustNotCall());
server.listen(0);
await once(server, 'listening');

const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const port = server.address().port;
const serverHost = `localhost:${port}`;

const agent = new http.Agent({
  proxyEnv: {
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  },
});

const req = http.request({ agent, host: 'localhost', port, path: '//foo/bar' });
req.end();
const [res] = await once(req, 'response');
res.setEncoding('utf8');
let body = '';
for await (const chunk of res) body += chunk;
assert.strictEqual(body, 'Hello world');

assert.strictEqual(logs.length, 1);
assert.strictEqual(logs[0].method, 'GET');
assert.strictEqual(logs[0].url, `http://${serverHost}//foo/bar`);
assert.strictEqual(logs[0].headers.host, serverHost);

server.close();
proxy.close();
agent.destroy();
