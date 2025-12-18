'use strict';
const common = require('../common');

const assert = require('assert');
const { Transform } = require('stream');

const t = new Transform();

assert.throws(
  () => {
    t.end(Buffer.from('blerg'));
  },
  {
    name: 'Error',
    code: 'ERR_METHOD_NOT_IMPLEMENTED',
    message: 'The _transform() method is not implemented'
  }
);

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

assert.strictEqual(t2._transform, _transform);
assert.strictEqual(t2._flush, _flush);
assert.strictEqual(t2._final, _final);

t2.end(Buffer.from('blerg'));
t2.resume();
