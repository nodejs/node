'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const Buffer = require('buffer').Buffer;
const fs = require('fs');
const filename = path.join(common.tmpDir, 'write.txt');
const expected = new Buffer('hello');
let openCalled = 0;
let writeCalled = 0;


common.refreshTmpDir();

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

