'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path'),
    fs = require('fs');

var file = path.join(common.tmpDir, 'write.txt');

var stream = fs.WriteStream(file),
    _fs_close = fs.close,
    _fs_open = fs.open;

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
