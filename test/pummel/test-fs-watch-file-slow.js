'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const FILENAME = path.join(common.tmpDir, 'watch-me');
const TIMEOUT = 1300;

let nevents = 0;

try {
  fs.unlinkSync(FILENAME);
} catch (e) {
  // swallow
}

fs.watchFile(FILENAME, {interval: TIMEOUT - 250}, function(curr, prev) {
  console.log([curr, prev]);
  switch (++nevents) {
    case 1:
      assert.strictEqual(common.fileExists(FILENAME), false);
      break;
    case 2:
    case 3:
      assert.strictEqual(common.fileExists(FILENAME), true);
      break;
    case 4:
      assert.strictEqual(common.fileExists(FILENAME), false);
      fs.unwatchFile(FILENAME);
      break;
    default:
      assert(0);
  }
});

process.on('exit', function() {
  assert.strictEqual(nevents, 4);
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
