'use strict';
const common = require('../common');
const assert = require('assert');

const Transform = require('stream').Transform;

const _transform = common.mustCall(function _transform(d, e, n) {
  n();
});

const _final = common.mustCall(function _final(n) {
  n();
});

const _flush = common.mustCall(function _flush(n) {
  n();
});

const t = new Transform({
  transform: _transform,
  flush: _flush,
  final: _final
});

const t2 = new Transform({});

t.end(Buffer.from('blerg'));
t.resume();

assert.throws(() => {
  t2.end(Buffer.from('blerg'));
}, /^Error: _transform\(\) is not implemented$/);


process.on('exit', () => {
  assert.strictEqual(t._transform, _transform);
  assert.strictEqual(t._flush, _flush);
  assert.strictEqual(t._final, _final);
});
