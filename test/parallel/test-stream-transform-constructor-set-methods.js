'use strict';
const common = require('../common');

const { strictEqual } = require('assert');
const { Transform } = require('stream');

const _transform = common.mustCall((chunk, _, next) => {
  next();
});

const _final = common.mustCall((next) => {
  next();
});

const _flush = common.mustCall((next) => {
  next();
});

const t = new Transform({
  transform: _transform,
  flush: _flush,
  final: _final,
});

strictEqual(t._transform, _transform);
strictEqual(t._flush, _flush);
strictEqual(t._final, _final);

t.end(Buffer.from('blerg'));
t.resume();

const t2 = new Transform({});

common.expectsError(() => {
  t2.end(Buffer.from('blerg'));
}, {
  type: Error,
  code: 'ERR_METHOD_NOT_IMPLEMENTED',
  message: 'The _transform method is not implemented',
});
