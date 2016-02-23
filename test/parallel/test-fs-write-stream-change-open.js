'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

var file = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

const stream = fs.WriteStream(file);
const _fs_close = fs.close;
const _fs_open = fs.open;

// change the fs.open with an identical function after the WriteStream
// has pushed it onto its internal action queue, but before it's
// returned.  This simulates AOP-style extension of the fs lib.
fs.open = function() {
  return _fs_open.apply(fs, arguments);
};

fs.close = function(fd) {
  assert.ok(fd, 'fs.close must not be called with an undefined fd.');
  fs.close = _fs_close;
  fs.open = _fs_open;
};

stream.write('foo');
stream.end();

process.on('exit', function() {
  assert.equal(fs.open, _fs_open);
});
