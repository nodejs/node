'use strict';
require('../common');
const assert = require('assert');

const Transform = require('stream').Transform;

let _transformCalled = false;
function _transform(d, e, n) {
  _transformCalled = true;
  n();
}

let _flushCalled = false;
function _flush(n) {
  _flushCalled = true;
  n();
}

const t = new Transform({
  transform: _transform,
  flush: _flush
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
  assert.strictEqual(_transformCalled, true);
  assert.strictEqual(_flushCalled, true);
});
