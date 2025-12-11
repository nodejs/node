'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

const file = tmpdir.resolve('write-autoclose-opt1.txt');
tmpdir.refresh();
let stream = fs.createWriteStream(file, { flags: 'w+', autoClose: false });
stream.write('Test1');
stream.end();
stream.on('finish', common.mustCall(function() {
  stream.on('close', common.mustNotCall());
  process.nextTick(common.mustCall(function() {
    assert.strictEqual(stream.closed, false);
    assert.notStrictEqual(stream.fd, null);
    next();
  }));
}));

function next() {
  // This will tell us if the fd is usable again or not
  stream = fs.createWriteStream(null, { fd: stream.fd, start: 0 });
  stream.write('Test2');
  stream.end();
  stream.on('finish', common.mustCall(function() {
    assert.strictEqual(stream.closed, false);
    stream.on('close', common.mustCall(function() {
      assert.strictEqual(stream.fd, null);
      assert.strictEqual(stream.closed, true);
      process.nextTick(next2);
    }));
  }));
}

function next2() {
  // This will test if after reusing the fd data is written properly
  fs.readFile(file, function(err, data) {
    assert.ifError(err);
    assert.strictEqual(data.toString(), 'Test2');
    process.nextTick(common.mustCall(next3));
  });
}

function next3() {
  // This is to test success scenario where autoClose is true
  const stream = fs.createWriteStream(file, { autoClose: true });
  stream.write('Test3');
  stream.end();
  stream.on('finish', common.mustCall(function() {
    assert.strictEqual(stream.closed, false);
    stream.on('close', common.mustCall(function() {
      assert.strictEqual(stream.fd, null);
      assert.strictEqual(stream.closed, true);
    }));
  }));
}

assert.throws(() => fs.WriteStream.prototype.autoClose, {
  code: 'ERR_INVALID_THIS',
});
