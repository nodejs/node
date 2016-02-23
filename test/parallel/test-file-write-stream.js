'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path');
var fs = require('fs');
var fn = path.join(common.tmpDir, 'write.txt');
common.refreshTmpDir();
var file = fs.createWriteStream(fn, {
  highWaterMark: 10
});

var EXPECTED = '012345678910';

var callbacks = {
  open: -1,
  drain: -2,
  close: -1
};

file
  .on('open', function(fd) {
    console.error('open!');
    callbacks.open++;
    assert.equal('number', typeof fd);
  })
  .on('error', function(err) {
    throw err;
  })
  .on('drain', function() {
    console.error('drain!', callbacks.drain);
    callbacks.drain++;
    if (callbacks.drain == -1) {
      assert.equal(EXPECTED, fs.readFileSync(fn, 'utf8'));
      file.write(EXPECTED);
    } else if (callbacks.drain == 0) {
      assert.equal(EXPECTED + EXPECTED, fs.readFileSync(fn, 'utf8'));
      file.end();
    }
  })
  .on('close', function() {
    console.error('close!');
    assert.strictEqual(file.bytesWritten, EXPECTED.length * 2);

    callbacks.close++;
    assert.throws(function() {
      console.error('write after end should not be allowed');
      file.write('should not work anymore');
    });

    fs.unlinkSync(fn);
  });

for (var i = 0; i < 11; i++) {
  (function(i) {
    file.write('' + i);
  })(i);
}

process.on('exit', function() {
  for (var k in callbacks) {
    assert.equal(0, callbacks[k], k + ' count off by ' + callbacks[k]);
  }
  console.log('ok');
});
