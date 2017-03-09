'use strict';
require('../common');
const assert = require('assert');

const Writable = require('stream').Writable;

let _writeCalled = false;
function _write(d, e, n) {
  _writeCalled = true;
}

const w = new Writable({ write: _write });
w.end(Buffer.from('blerg'));

let _writevCalled = false;
let dLength = 0;
function _writev(d, n) {
  dLength = d.length;
  _writevCalled = true;
}

const w2 = new Writable({ writev: _writev });
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
