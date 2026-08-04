// This tests that proxied HTTP requests fail validation before sending any data.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer } from '../common/proxy-server.js';

const target = http.createServer(common.mustNotCall());
target.listen(0);
await once(target, 'listening');
target.on('error', common.mustNotCall());

const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const agent = new http.Agent({
  proxyEnv: {
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  },
});

const port = target.address().port;
const base = { host: 'localhost', port, path: '/test', agent };

function throwsWith(message, cases) {
  for (const { name, options } of cases) {
    assert.throws(() => {
      http.request(options, common.mustNotCall());
    }, { code: 'ERR_INVALID_ARG_VALUE', message }, name);
  }
}

// Path authority or Host header disagrees with options.host:port.
throwsWith(/must match the request authority/, [
  { name: 'absolute path with different host',
    options: { ...base, path: 'http://example.test/test' } },
  { name: 'object Host header with different host',
    options: { ...base, headers: { Host: 'bad.example' } } },
  { name: 'object Host header with different port',
    options: { ...base, headers: { Host: 'localhost:1' } } },
  { name: 'array Host header with different host',
    options: { ...base, headers: ['Host', 'bad.example'] } },
  { name: 'array-of-pairs Host header with different host',
    options: { ...base, headers: [['Host', 'bad.example']] } },
  { name: 'array Host header omitting port for non-default port',
    options: { ...base, headers: ['Host', 'localhost'] } },
  { name: 'absolute path omitting port for non-default port',
    options: { ...base, path: 'http://localhost/test' } },
  { name: 'object Host header omitting port when defaultPort suppresses it',
    options: { ...base, defaultPort: port, headers: { Host: 'localhost' } } },
]);

// Absolute-form path uses a non-http scheme.
throwsWith(/must use http: scheme/, [
  { name: 'absolute path with https: scheme',
    options: { ...base, path: `https://localhost:${port}/test` } },
]);

// Absolute-form path contains userinfo.
throwsWith(/must not contain userinfo/, [
  { name: 'absolute path with userinfo',
    options: { ...base, path: `http://user:pass@localhost:${port}/test` } },
]);

// Origin-form path must start with /.
throwsWith(/must be in absolute-form or start with \//, [
  { name: 'path without leading slash but includes @',
    options: { ...base, path: '@other.example/test' } },
]);

// Host header value is not a bare authority (contains userinfo, path, query, or fragment).
throwsWith(/must match the request authority/, [
  { name: 'Host with userinfo',
    options: { ...base, headers: { Host: `user@localhost:${port}` } } },
  { name: 'Host with path',
    options: { ...base, headers: { Host: `localhost:${port}/path` } } },
  { name: 'Host with query',
    options: { ...base, headers: { Host: `localhost:${port}?x` } } },
  { name: 'Host with fragment',
    options: { ...base, headers: { Host: `localhost:${port}#x` } } },
]);

// Multiple Host headers (invalid per RFC 9110 Section 5.3).
throwsWith(/must not contain duplicate Host headers/, [
  { name: 'flat array with duplicate Host',
    options: { ...base, headers: ['Host', `localhost:${port}`, 'Host', 'bad.example'] } },
  { name: 'array-of-pairs with duplicate Host',
    options: { ...base, headers: [['Host', `localhost:${port}`], ['Host', 'bad.example']] } },
  { name: 'case-insensitive duplicate Host',
    options: { ...base, headers: ['host', `localhost:${port}`, 'HOST', 'bad.example'] } },
]);

// Non-array entry in array-of-pairs form.
throwsWith(/must be an array when headers is passed as an array of pairs/, [
  { name: 'object with numeric keys smuggling a Host',
    options: { ...base, headers: [['Host', `localhost:${port}`], { 0: 'Host', 1: 'bad.example' }] } },
]);

// Invalid port.
for (const { name, badPort } of [
  { name: 'port >= 65536', badPort: 99999 },
  { name: 'port === 65536', badPort: 65536 },
]) {
  assert.throws(() => {
    http.request({ ...base, port: badPort }, common.mustNotCall());
  }, { code: 'ERR_SOCKET_BAD_PORT' }, name);
}

assert.throws(() => {
  http.request({ ...base, method: 'GET', path: '*' }, common.mustNotCall());
}, { code: 'ERR_INVALID_ARG_VALUE', message: /must be in absolute-form or start with \// });

assert.deepStrictEqual(logs, []);

target.close();
proxy.close();
agent.destroy();
