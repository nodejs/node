'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const http = require('http');

async function main() {
  const seen = [];
  let responseClose;

  const server = http.createServer(common.mustCall((req, res) => {
    responseClose = new Promise((resolve) => res.once('close', resolve));

    res.on('finish', common.mustNotCall());
    res.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
      assert.strictEqual(err.message, 'aborted');
      seen.push('error');
    }));
    res.on('close', common.mustCall(() => {
      seen.push('close');
    }));

    req.socket.destroy();
    setImmediate(() => res.end('unreachable'));
  }));

  server.listen(0, '127.0.0.1');
  await once(server, 'listening');

  const req = http.get(`http://127.0.0.1:${server.address().port}/`);
  await once(req, 'error');
  await responseClose;

  server.close();
  await once(server, 'close');

  assert.deepStrictEqual(seen, ['error', 'close']);
}

main().then(common.mustCall());
