'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const tty = require('tty');

// Check that the bytesWritten getter doesn't crash if object isn't
// constructed.
assert.strictEqual(net.Socket.prototype.bytesWritten, undefined);
assert.strictEqual(Object.getPrototypeOf(tls.TLSSocket).prototype.bytesWritten,
                   undefined);
assert.strictEqual(tls.TLSSocket.prototype.bytesWritten, undefined);
assert.strictEqual(Object.getPrototypeOf(tty.ReadStream).prototype.bytesWritten,
                   undefined);
assert.strictEqual(tty.ReadStream.prototype.bytesWritten, undefined);
assert.strictEqual(tty.WriteStream.prototype.bytesWritten, undefined);
