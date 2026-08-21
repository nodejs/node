'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const events = require('events');
const fixtures = require('../common/fixtures');
const { createServer, connect } = require('tls');
const cert = fixtures.readKey('rsa_cert.crt');
const key = fixtures.readKey('rsa_private.pem');

events.captureRejections = true;

const server = createServer({ cert, key }, common.mustCall(async (sock) => {
  server.close();

  const _err = new Error('kaboom');
  sock.on('error', common.mustCall((err) => {
    assert.strictEqual(err, _err);
  }));
  throw _err;
}));

server.listen(0, common.mustCall(() => {
  const sock = connect({
    port: server.address().port,
    host: server.address().host,
    rejectUnauthorized: false
  });

  sock.on('close', common.mustCall());
}));
