'use strict';

const common = require('../common');

// Test the `allowHalfOpen` option of the `tls.TLSSocket` constructor.

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const stream = require('stream');
const tls = require('tls');

{
  // The option is ignored when the `socket` argument is a `net.Socket`.
  const socket = new tls.TLSSocket(new net.Socket(), { allowHalfOpen: true });
  assert.strictEqual(socket.allowHalfOpen, false);
}

{
  // The option is ignored when the `socket` argument is a generic
  // `stream.Duplex`.
  const duplex = new stream.Duplex({
    allowHalfOpen: false,
    read() {}
  });
  const socket = new tls.TLSSocket(duplex, { allowHalfOpen: true });
  assert.strictEqual(socket.allowHalfOpen, false);
}

{
  const socket = new tls.TLSSocket();
  assert.strictEqual(socket.allowHalfOpen, false);
}

{
  // The option is honored when the `socket` argument is not specified.
  const socket = new tls.TLSSocket(undefined, { allowHalfOpen: true });
  assert.strictEqual(socket.allowHalfOpen, true);
}
