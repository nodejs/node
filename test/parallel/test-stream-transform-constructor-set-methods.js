'use strict';
require('../common');
var assert = require('assert');

var Transform = require('stream').Transform;

var _transformCalled = false;
function _transform(d, e, n) {
  _transformCalled = true;
  n();
}

var _flushCalled = false;
function _flush(n) {
  _flushCalled = true;
  n();
}

var t = new Transform({
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
