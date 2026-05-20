'use strict';

const common = require('../common');
const assert = require('assert');
const events = require('events');
const { createServer, connect } = require('net');

events.captureRejections = true;

const server = createServer(common.mustCall(async (sock) => {
  server.close();

  const _err = new Error('kaboom');
  sock.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
  }));
  throw _err;
}));

server.listen(0, common.mustCall(() => {
  const sock = connect(
    server.address().port,
    server.address().host
  );

  sock.on('close', common.mustCall());
}));
