'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path'),
    Buffer = require('buffer').Buffer,
    fs = require('fs'),
    filename = path.join(common.tmpDir, 'write.txt'),
    expected = new Buffer('hello'),
    openCalled = 0,
    writeCalled = 0;


fs.open(filename, 'w', 0o644, function(err, fd) {
  openCalled++;
  if (err) throw err;

  fs.write(fd, expected, 0, expected.length, null, function(err, written) {
    writeCalled++;
    if (err) throw err;

    assert.equal(expected.length, written);
    fs.closeSync(fd);

    var found = fs.readFileSync(filename, 'utf8');
    assert.deepEqual(expected.toString(), found);
    fs.unlinkSync(filename);
  });
});

process.on('exit', function() {
  assert.equal(1, openCalled);
  assert.equal(1, writeCalled);
});

