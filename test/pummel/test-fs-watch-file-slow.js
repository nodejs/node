'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var FILENAME = path.join(common.tmpDir, 'watch-me');
var TIMEOUT = 1300;

var nevents = 0;

try {
  fs.unlinkSync(FILENAME);
} catch (e) {
  // swallow
}

fs.watchFile(FILENAME, {interval: TIMEOUT - 250}, function(curr, prev) {
  console.log([curr, prev]);
  switch (++nevents) {
    case 1:
      assert.equal(common.fileExists(FILENAME), false);
      break;
    case 2:
    case 3:
      assert.equal(common.fileExists(FILENAME), true);
      break;
    case 4:
      assert.equal(common.fileExists(FILENAME), false);
      fs.unwatchFile(FILENAME);
      break;
    default:
      assert(0);
  }
});

process.on('exit', function() {
  assert.equal(nevents, 4);
});

setTimeout(createFile, TIMEOUT);

function createFile() {
  console.log('creating file');
  fs.writeFileSync(FILENAME, 'test');
  setTimeout(touchFile, TIMEOUT);
}

function touchFile() {
  console.log('touch file');
  fs.writeFileSync(FILENAME, 'test');
  setTimeout(removeFile, TIMEOUT);
}

function removeFile() {
  console.log('remove file');
  fs.unlinkSync(FILENAME);
}
