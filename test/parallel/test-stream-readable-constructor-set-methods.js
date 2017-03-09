'use strict';
require('../common');
const assert = require('assert');

const Readable = require('stream').Readable;

let _readCalled = false;
function _read(n) {
  _readCalled = true;
  this.push(null);
}

const r = new Readable({ read: _read });
r.resume();

process.on('exit', function() {
  assert.equal(r._read, _read);
  assert(_readCalled);
});
