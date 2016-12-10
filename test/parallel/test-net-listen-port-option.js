'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

function close() { this.close(); }

// From lib/net.js
function toNumber(x) { return (x = Number(x)) >= 0 ? x : false; }

function isPipeName(s) {
  return typeof s === 'string' && toNumber(s) === false;
}

const listenVariants = [
  (port, cb) => net.Server().listen({port}, cb),
  (port, cb) => net.Server().listen(port, cb)
];

listenVariants.forEach((listenVariant, i) => {
  listenVariant(undefined, common.mustCall(close));
  listenVariant('0', common.mustCall(close));

  [
    'nan',
    -1,
    123.456,
    0x10000,
    1 / 0,
    -1 / 0,
    '+Infinity',
    '-Infinity'
  ].forEach((port) => {
    if (i === 1 && isPipeName(port)) {
      // skip this, because listen(port) can also be listen(path)
      return;
    }
    assert.throws(() => listenVariant(port, common.fail),
                  /"port" argument must be >= 0 and < 65536/i);
  });

  [null, true, false].forEach((port) =>
    assert.throws(() => listenVariant(port, common.fail),
                  /invalid listen argument/i));
});
