// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const JSStreamWrap = require('internal/wrap_js_stream');
const { Duplex } = require('stream');

process.once('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'exception!');
}));

const socket = new JSStreamWrap(new Duplex({
  read: common.mustCall(),
  write: common.mustCall((buffer, data, cb) => {
    throw new Error('exception!');
  })
}));

assert.throws(() => socket.end('foo'), /Error: write EPROTO/);
