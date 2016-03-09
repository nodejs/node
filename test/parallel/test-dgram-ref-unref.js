'use strict';
require('../common');
const dgram = require('dgram');
const assert = require('assert');

const sock4 = dgram.createSocket('udp4');
assert.ok(sock4.hasOwnProperty('_unref'));
assert.ok(!sock4._unref);
sock4.unref();
assert.ok(sock4._unref);
sock4.ref();
assert.ok(!sock4._unref);
sock4.unref();
assert.ok(sock4._unref);

const sock6 = dgram.createSocket('udp6');
assert.ok(sock6.hasOwnProperty('_unref'));
assert.ok(!sock6._unref);
sock6.unref();
assert.ok(sock6._unref);
sock6.ref();
assert.ok(!sock6._unref);
sock6.unref();
assert.ok(sock6._unref);
