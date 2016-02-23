'use strict';

require('../common');
const assert = require('assert');
const net = require('net');
const tls = require('tls');
const tty = require('tty');

// Check that the bytesWritten getter doesn't crash if object isn't
// constructed.
assert.strictEqual(net.Socket.prototype.bytesWritten, undefined);
assert.strictEqual(tls.TLSSocket.super_.prototype.bytesWritten, undefined);
assert.strictEqual(tls.TLSSocket.prototype.bytesWritten, undefined);
assert.strictEqual(tty.ReadStream.super_.prototype.bytesWritten, undefined);
assert.strictEqual(tty.ReadStream.prototype.bytesWritten, undefined);
assert.strictEqual(tty.WriteStream.prototype.bytesWritten, undefined);
