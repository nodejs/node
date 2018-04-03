'use strict';
const common = require('../common');

const { strictEqual } = require('assert');
const { Transform } = require('stream');

const t = new Transform();

t.on('error', common.expectsError({
  type: Error,
  code: 'ERR_METHOD_NOT_IMPLEMENTED',
  message: 'The _transform() method is not implemented'
}));

t.end(Buffer.from('blerg'));

const _transform = common.mustCall((chunk, _, next) => {
  next();
});

const _final = common.mustCall((next) => {
  next();
});

const _flush = common.mustCall((next) => {
  next();
});

const t2 = new Transform({
  transform: _transform,
  flush: _flush,
  final: _final
});

strictEqual(t2._transform, _transform);
strictEqual(t2._flush, _flush);
strictEqual(t2._final, _final);

t2.end(Buffer.from('blerg'));
t2.resume();
