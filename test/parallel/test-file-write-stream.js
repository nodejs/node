'use strict';
const common = require('../common');
const assert = require('assert');

const path = require('path');
const fs = require('fs');
const fn = path.join(common.tmpDir, 'write.txt');
common.refreshTmpDir();
const file = fs.createWriteStream(fn, {
  highWaterMark: 10
});

const EXPECTED = '012345678910';

const callbacks = {
  open: -1,
  drain: -2,
  close: -1
};

file
  .on('open', function(fd) {
    console.error('open!');
    callbacks.open++;
    assert.strictEqual('number', typeof fd);
  })
  .on('error', function(err) {
    throw err;
  })
  .on('drain', function() {
    console.error('drain!', callbacks.drain);
    callbacks.drain++;
    if (callbacks.drain === -1) {
      assert.strictEqual(EXPECTED, fs.readFileSync(fn, 'utf8'));
      file.write(EXPECTED);
    } else if (callbacks.drain === 0) {
      assert.strictEqual(EXPECTED + EXPECTED, fs.readFileSync(fn, 'utf8'));
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

for (let i = 0; i < 11; i++) {
  file.write(`${i}`);
}

process.on('exit', function() {
  for (const k in callbacks) {
    assert.strictEqual(0, callbacks[k], `${k} count off by ${callbacks[k]}`);
  }
  console.log('ok');
});
