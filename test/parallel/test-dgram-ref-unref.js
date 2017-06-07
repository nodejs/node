'use strict';
require('../common');
const dgram = require('dgram');
const assert = require('assert');

const sock4 = dgram.createSocket('udp4');
assert.strictEqual(sock4.hasOwnProperty('_unref'), true);
assert.strictEqual(sock4._unref, false);
sock4.unref();
assert.strictEqual(sock4._unref, true);
sock4.ref();
assert.strictEqual(sock4._unref, false);
sock4.unref();
assert.strictEqual(sock4._unref, true);

const sock6 = dgram.createSocket('udp6');
assert.strictEqual(sock6.hasOwnProperty('_unref'), true);
assert.strictEqual(sock6._unref, false);
sock6.unref();
assert.strictEqual(sock6._unref, true);
sock6.ref();
assert.strictEqual(sock6._unref, false);
sock6.unref();
assert.strictEqual(sock6._unref, true);
