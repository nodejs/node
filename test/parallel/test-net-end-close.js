// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const { internalBinding } = require('internal/test/binding');
const { UV_EOF } = internalBinding('uv');

const s = new net.Socket({
  handle: {
    readStart: function() {
      setImmediate(() => this.onread(UV_EOF, null));
    },
    close: (cb) => setImmediate(cb)
  },
  writable: false
});
assert.strictEqual(s, s.resume());

const events = [];

s.on('end', () => {
  events.push('end');
});
s.on('close', () => {
  events.push('close');
});

process.on('exit', () => {
  assert.deepStrictEqual(events, [ 'end', 'close' ]);
});
