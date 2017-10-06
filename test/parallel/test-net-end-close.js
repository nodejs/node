'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const uv = process.binding('uv');

const s = new net.Socket({
  handle: {
    readStart: function() {
      process.nextTick(() => this.onread(uv.UV_EOF, null));
    },
    close: (cb) => process.nextTick(cb)
  },
  writable: false
});
assert.strictEqual(s, s.resume());

const events = [];

s.on('end', () => events.push('end'));
s.on('close', () => events.push('close'));

process.on('exit', () => {
  assert.deepStrictEqual(events, [ 'end', 'close' ]);
});
