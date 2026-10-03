'use strict';

// This tests that a file-backed Blob whose size exceeds the range of a 32-bit
// unsigned integer reports its real size, can be sliced past that boundary,
// and survives a round trip through URL.createObjectURL().

const common = require('../common');

common.skipIf32Bits();

const assert = require('assert');
const fs = require('fs');
const { resolveObjectURL } = require('buffer');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filepath = tmpdir.resolve('large-blob.bin');
const size = 5 * 1024 * 1024 * 1024;  // 5GB, sparse
const message = 'Large File';

try {
  const fd = fs.openSync(filepath, 'w+');
  fs.ftruncateSync(fd, size);
  const buf = Buffer.from(message);
  fs.writeSync(fd, buf, 0, buf.length, size - buf.length);
  fs.closeSync(fd);
} catch (e) {
  if (e.code !== 'ENOSPC') {
    throw e;
  }
  common.skip('insufficient disk space');
}

(async () => {
  const blob = await fs.openAsBlob(filepath);
  assert.strictEqual(blob.size, size);

  const tail = blob.slice(size - message.length, size);
  assert.strictEqual(tail.size, message.length);
  assert.strictEqual(await tail.text(), message);

  const url = URL.createObjectURL(blob);
  try {
    assert.strictEqual(resolveObjectURL(url).size, size);
  } finally {
    URL.revokeObjectURL(url);
  }
})().then(common.mustCall());
