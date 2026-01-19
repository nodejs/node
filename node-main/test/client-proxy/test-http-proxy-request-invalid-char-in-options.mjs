// This tests that invalid hosts or ports with carriage return or newline characters
// in HTTP request options would lead to ERR_INVALID_CHAR.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';

const server = http.createServer(common.mustNotCall());
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

const proxy = http.createServer(common.mustNotCall());
proxy.listen(0);
await once(proxy, 'listening');

const agent = new http.Agent({
  proxyEnv: {
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  },
});

for (const testCase of testCases) {
  const options = { ...testCase, agent };
  assert.throws(() => {
    http.request(options, common.mustNotCall());
  }, {
    code: 'ERR_INVALID_CHAR',
  });
}

server.close();
proxy.close();
