'use strict';
const common = require('../common');

// This test ensures that the `'close'` event is emitted after the `'error'`
// event when a request is made and the socket is closed before we started to
// receive a response.

const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const req = http.get({ port: server.address().port }, common.mustNotCall());
  let errorEmitted = false;

  req.on('error', (err) => {
    errorEmitted = true;
    assert.strictEqual(err.constructor, Error);
    assert.strictEqual(err.message, 'socket hang up');
    assert.strictEqual(err.code, 'ECONNRESET');
  });

  req.on('close', common.mustCall(() => {
    assert.strictEqual(errorEmitted, true);
    server.close();
  }));

  req.destroy();
}));
