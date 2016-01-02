'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const Buffer = require('buffer').Buffer;
const kStringMaxLength = process.binding('buffer').kStringMaxLength;

var fd;

common.refreshTmpDir();

const file = path.join(common.tmpDir, 'toobig2.txt');
const stream = fs.createWriteStream(file, {
  flags: 'a'
});

const size = kStringMaxLength / 200;
const a = new Buffer(size).fill('a');

for (var i = 0; i < 201; i++) {
  stream.write(a);
}

stream.end();
stream.on('finish', common.mustCall(function() {
  fd = fs.openSync(file, 'r');
  fs.read(fd, kStringMaxLength + 1, 0, 'utf8', common.mustCall(function(err) {
    assert.ok(err instanceof Error);
    assert.strictEqual('toString failed', err.message);
  }));
}));

function destroy() {
  try {
    // Make sure we close fd and unlink the file
    fs.closeSync(fd);
    fs.unlinkSync(file);
  } catch (err) {
    // it may not exist
  }
}

process.on('exit', destroy);

process.on('SIGINT', function() {
  destroy();
  process.exit();
});

// To make sure we don't leave a very large file
// on test machines in the event this test fails.
process.on('uncaughtException', function(err) {
  destroy();
  throw err;
});
