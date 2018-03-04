'use strict';
const common = require('../common');

const { strictEqual } = require('assert');
const { Writable } = require('stream');

const _write = common.mustCall((chunk, _, next) => {
  next();
});

const _writev = common.mustCall((chunks, next) => {
  strictEqual(chunks.length, 2);
  next();
});

const w = new Writable({ write: _write, writev: _writev });

strictEqual(w._write, _write);
strictEqual(w._writev, _writev);

w.write(Buffer.from('blerg'));

w.cork();
w.write(Buffer.from('blerg'));
w.write(Buffer.from('blerg'));

w.end();

const w2 = new Writable();

w2.on('error', common.expectsError({
  type: Error,
  code: 'ERR_METHOD_NOT_IMPLEMENTED',
  message: 'The _write method is not implemented',
}));

w2.end(Buffer.from('blerg'));
