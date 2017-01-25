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

t.end(Buffer.from('blerg'));
t.resume();

process.on('exit', function() {
  assert.equal(t._transform, _transform);
  assert.equal(t._flush, _flush);
  assert(_transformCalled);
  assert(_flushCalled);
});
