'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
const file = path.join(tmpdir.path, 'out.txt');

tmpdir.refresh();

{
  const _fs_write = fs.write;
  let i = 0;
  fs.write = function(fd, data, offset, length, pos, cb) {
    if (i === 0) {
      const half = Math.floor(length / 2);
      _fs_write(fd, data, offset, half, pos, cb);
    } else {
      assert(
        offset > 0,
        `second call to fs.write() must provide non-zero offset, got ${offset}`
      );
      _fs_write(fd, data, offset, length, pos, cb);
    }
    i++;
  };

  const stream = fs.createWriteStream(file);
  const data = Buffer.from('Hello World, how are you?', 'ascii');
  stream.write(data, 'ascii', common.mustCall(function(err) {
    assert.ifError(err);
  }));
  stream.end();

  process.on('exit', function() {
    assert.strictEqual(
      i, 2, 'expected fs.write() to be called twice but got ' + i);
    const content = fs.readFileSync(file);
    assert.strictEqual(content.toString(), 'Hello World, how are you?');
  });
}
