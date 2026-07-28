// This tests that invalid hosts or ports with carriage return or newline characters
// in HTTPsS request options would lead to ERR_INVALID_CHAR.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import fixtures from '../common/fixtures.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustNotCall());
server.listen(0);
await once(server, 'listening');
server.on('error', common.mustNotCall());
const port = server.address().port.toString();

const testCases = [
  { host: 'local\rhost', port: port, path: '/carriage-return-in-host' },
  { host: 'local\nhost', port: port, path: '/newline-in-host' },
  { host: 'local\r\nhost', port: port, path: '/crlf-in-host' },
  { host: 'localhost', port: port.substring(0, 1) + '\r' + port.substring(1), path: '/carriage-return-in-port' },
  { host: 'localhost', port: port.substring(0, 1) + '\n' + port.substring(1), path: '/newline-in-port' },
  { host: 'localhost', port: port.substring(0, 1) + '\r\n' + port.substring(1), path: '/crlf-in-port' },
];

const proxy = https.createServer(common.mustNotCall());
proxy.listen(0);
await once(proxy, 'listening');

const agent = new https.Agent({
  ca: fixtures.readKey('fake-startcom-root-cert.pem'),
  proxyEnv: {
    HTTPS_PROXY: `https://localhost:${proxy.address().port}`,
  },
});

for (const testCase of testCases) {
  const options = { ...testCase, agent };
  assert.throws(() => {
    https.request(options, common.mustNotCall());
  }, {
    code: 'ERR_INVALID_CHAR',
  });
}

server.close();
proxy.close();
