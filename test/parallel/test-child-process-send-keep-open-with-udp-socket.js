// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const dgram = require('dgram');
const { kStateSymbol } = require('internal/dgram');
const { internalBinding } = require('internal/test/binding');
const { UDP } = internalBinding('udp_wrap');

const count = 6;

if (process.argv[2] !== 'child') {
  const child = cp.fork(__filename, ['child']);

  // The first step
  const socket1 = dgram.createSocket('udp4')
    .bind(0, common.mustCall(() => {
      child.send(null, socket1[kStateSymbol].handle, { keepOpen: true }, common.mustSucceed());
    }));

  const socket2 = dgram.createSocket('udp4')
    .bind(0, common.mustCall(() => {
      child.send(null, socket2[kStateSymbol].handle, { keepOpen: false }, common.mustSucceed());
    }));

  const socket3 = dgram.createSocket('udp4')
    .bind(0, common.mustCall(() => {
      // The default value of `keepOpen` is true
      child.send(null, socket3[kStateSymbol].handle, common.mustSucceed());
    }));

  const socket4 = dgram.createSocket('udp4')
    .bind(0, common.mustCall(() => {
      child.send(null, socket4, { keepOpen: true }, common.mustSucceed());
    }));

  const socket5 = dgram.createSocket('udp4')
    .bind(0, common.mustCall(() => {
      child.send(null, socket5, { keepOpen: false }, common.mustSucceed());
    }));

  const socket6 = dgram.createSocket('udp4')
    .bind(0, common.mustCall(() => {
      // The default value of `keepOpen` is true
      child.send(null, socket6, common.mustSucceed());
    }));

  let i = 0;

  // The third step
  child.on('message', common.mustCall((msg) => {
    if (++i === count) {
      child.disconnect();
    }
  }, count));

  // The fourth step
  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    // Test if the socket is closed
    assert.strictEqual(socket1.address().port > 0, true);
    assert.throws(() => socket2.address(), Error);
    assert.strictEqual(socket3.address().port > 0, true);
    assert.strictEqual(socket4.address().port > 0, true);
    assert.throws(() => socket5.address(), Error);
    assert.strictEqual(socket6.address().port > 0, true);
    socket1.close();
    socket3.close();
    socket4.close();
    socket6.close();
  }));
} else {
  // The second step
  process.on('message', common.mustCall((_, handle) => {
    assert.strictEqual(handle instanceof UDP || handle instanceof dgram.Socket, true);
    handle.close(common.mustCall(() => {
      process.send('done');
    }));
  }, count));
}
