'use strict';

const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');

// Regression test: an ordinary object that happens to satisfy `isURL()` (truthy
// `href`/`protocol`, with `auth`/`path` absent) must not be able to smuggle
// HTTP request options such as `socketPath`, `method`, `headers`, or `agent`
// through `urlToHttpOptions()`. Doing so would let an attacker redirect an
// otherwise hostname-validated request to a Unix domain socket (e.g. the
// Docker daemon) and defeat hostname-based SSRF defenses.

const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req.method, 'GET'); // `method` must not be smuggled in.
  assert.strictEqual(req.headers['x-injected'], undefined); // `headers` too.
  assert.strictEqual(req.url, '/ping');
  res.writeHead(200);
  res.end('pong');
}));

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  const { port } = server.address();
  // A duck-typed object that `isURL()` would accept, but which is not a genuine
  // WHATWG URL instance.
  const duckTypedUrl = {
    href: 'http://127.0.0.1/verify',
    protocol: 'http:',
    hostname: '127.0.0.1',
    pathname: '/ping',
    port: String(port),
    username: '',
    password: '',
    search: '',
    hash: '',
    socketPath: path.join(tmpdir.path, 'should-not-be-used.sock'),
    method: 'POST',
    headers: { 'x-injected': 'yes' },
    agent: false,
  };
  const req = http.request(duckTypedUrl, common.mustCall((res) => {
    let body = '';
    res.on('data', (chunk) => { body += chunk; });
    res.on('end', common.mustCall(() => {
      assert.strictEqual(body, 'pong');
      server.close();
    }));
  }));
  req.on('error', common.mustNotCall(
    'request connected to the injected socketPath instead of the hostname'));
  req.end();
}));
