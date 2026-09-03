'use strict';
const common = require('../common');

// Walking the `_parent` chain must stop on a nullish link, not only strict
// `null`. During connection teardown a socket's `_parent` can be left
// `undefined`, which previously caused `_unrefTimer()` and `_destroy()` to read
// a property off `undefined` and throw.
// Refs: https://github.com/nodejs/node/issues/64490

const assert = require('assert');
const net = require('net');

{
  const socket = new net.Socket();
  socket._parent = undefined;
  socket._unrefTimer();
}

{
  const socket = new net.Socket();
  socket._parent = undefined;
  socket.on('error', common.mustNotCall());
  socket.destroy();
  assert.strictEqual(socket.destroyed, true);
}
