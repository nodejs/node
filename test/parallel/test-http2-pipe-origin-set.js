'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// Without a servername the origin set falls back to the peer address, which
// for a pipe is a path rather than a host and must not be used as one.

function test(path) {
  const server = http2.createSecureServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  });
  server.on('stream', common.mustCall((stream) => {
    stream.respond();
    stream.end();
  }));
  server.listen(path, common.mustCall(() => {
    const client = http2.connect('https://127.0.0.1', {
      path,
      rejectUnauthorized: false,
    });
    client.on('connect', common.mustCall(() => {
      assert.strictEqual(client.socket.servername, false);
      assert.deepStrictEqual(client.originSet, ['https://localhost']);
      const req = client.request();
      req.resume();
      req.on('close', common.mustCall(() => {
        client.close();
        server.close();
      }));
    }));
  }));
}

test(`${common.PIPE}-origin-set`);

if (common.isLinux) {
  test(`\0node-test-origin-set-${process.pid}`);
}
