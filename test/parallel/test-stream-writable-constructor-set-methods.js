'use strict';
require('../common');
var assert = require('assert');

var Writable = require('stream').Writable;

var _writeCalled = false;
function _write(d, e, n) {
  _writeCalled = true;
}

var w = new Writable({ write: _write });
w.end(Buffer.from('blerg'));

var _writevCalled = false;
var dLength = 0;
function _writev(d, n) {
  dLength = d.length;
  _writevCalled = true;
}

var w2 = new Writable({ writev: _writev });
w2.cork();

w2.write(Buffer.from('blerg'));
w2.write(Buffer.from('blerg'));
w2.end();

process.on('exit', function() {
  assert.equal(w._write, _write);
  assert(_writeCalled);
  assert.equal(w2._writev, _writev);
  assert.equal(dLength, 2);
  assert(_writevCalled);
});
