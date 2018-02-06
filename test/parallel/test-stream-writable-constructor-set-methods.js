'use strict';
const common = require('../common');
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

const w3 = new Writable();

w3.on('error', common.expectsError({
  type: Error,
  code: 'ERR_METHOD_NOT_IMPLEMENTED',
  message: 'The _write method is not implemented'
}));

w3.end(Buffer.from('blerg'));

process.on('exit', function() {
  assert.strictEqual(w._write, _write);
  assert(_writeCalled);
  assert.strictEqual(w2._writev, _writev);
  assert.strictEqual(dLength, 2);
  assert(_writevCalled);
});
