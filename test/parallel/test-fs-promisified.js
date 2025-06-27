'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { promisify } = require('util');

const read = promisify(fs.read);
const write = promisify(fs.write);
const exists = promisify(fs.exists);

{
  const fd = fs.openSync(__filename, 'r');
  read(fd, Buffer.alloc(1024), 0, 1024, null).then(common.mustCall((obj) => {
    assert.strictEqual(typeof obj.bytesRead, 'number');
    assert(obj.buffer instanceof Buffer);
    fs.closeSync(fd);
  }));
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
{
  const filename = tmpdir.resolve('write-promise.txt');
  const fd = fs.openSync(filename, 'w');
  write(fd, Buffer.from('foobar')).then(common.mustCall((obj) => {
    assert.strictEqual(typeof obj.bytesWritten, 'number');
    assert.strictEqual(obj.buffer.toString(), 'foobar');
    fs.closeSync(fd);
  }));
}

{
  exists(__filename).then(common.mustCall((x) => {
    assert.strictEqual(x, true);
  }));
}
