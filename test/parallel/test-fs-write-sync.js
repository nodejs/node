'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const filename = path.join(common.tmpDir, 'write.txt');

common.refreshTmpDir();

// fs.writeSync with all parameters provided:
{
  const fd = fs.openSync(filename, 'w');

  let written = fs.writeSync(fd, '');
  assert.strictEqual(0, written);

  fs.writeSync(fd, 'foo');

  written = fs.writeSync(fd, Buffer.from('bár'), 0, Buffer.byteLength('bár'));
  assert.ok(written > 3);
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');
}

// fs.writeSync with a buffer, without the length parameter:
{
  const fd = fs.openSync(filename, 'w');

  let written = fs.writeSync(fd, '');
  assert.strictEqual(0, written);

  fs.writeSync(fd, 'foo');

  written = fs.writeSync(fd, Buffer.from('bár'), 0);
  assert.ok(written > 3);
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');
}

// fs.writeSync with a buffer, without the offset and length parameters:
{
  const fd = fs.openSync(filename, 'w');

  let written = fs.writeSync(fd, '');
  assert.strictEqual(0, written);

  fs.writeSync(fd, 'foo');

  written = fs.writeSync(fd, Buffer.from('bár'));
  assert.ok(written > 3);
  fs.closeSync(fd);

  assert.strictEqual(fs.readFileSync(filename, 'utf-8'), 'foobár');
}
