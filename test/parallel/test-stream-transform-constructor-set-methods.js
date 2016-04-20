'use strict';
require('../common');
var assert = require('assert');

var Transform = require('stream').Transform;

var _transformCalled = false;
function _transform(d, e, n) {
  _transformCalled = true;
  n();
}

var _endCalled = false;
function _end(n) {
  _endCalled = true;
  n();
}

var t = new Transform({
  transform: _transform,
  end: _end
});

t.end(Buffer.from('blerg'));
t.resume();

process.on('exit', function() {
  assert.equal(t._transform, _transform);
  assert.equal(t._end, _end);
  assert(_transformCalled);
  assert(_endCalled);
});
