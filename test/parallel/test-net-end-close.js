'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const uv = process.binding('uv');

const s = new net.Socket({
  handle: {
    readStart: function() {
      process.nextTick(() => { return this.onread(uv.UV_EOF, null); });
    },
    close: (cb) => { return process.nextTick(cb); }
  },
  writable: false
});
s.resume();

const events = [];

s.on('end', () => { return events.push('end'); });
s.on('close', () => { return events.push('close'); });

process.on('exit', () => {
  assert.deepStrictEqual(events, [ 'end', 'close' ]);
});
