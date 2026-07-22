'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const { join } = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// ✅ Test that both ReadStream and WriteStream support the singular 'flag' option
{
  const testFile = join(tmpdir.path, 'test-flag-read.txt');
  fs.writeFileSync(testFile, 'hello world');

  // --- Test ReadStream ---
  const readStream = fs.createReadStream(testFile, { flag: 'r' });
  assert.strictEqual(readStream.flags, 'r');
  readStream.close();

  // --- Test WriteStream ---
  const writeFile = join(tmpdir.path, 'test-flag-write.txt');
  const writeStream = fs.createWriteStream(writeFile, { flag: 'a' });
  assert.strictEqual(writeStream.flags, 'a');
  writeStream.end('hello');

  writeStream.on('close', common.mustCall(() => {
    const content = fs.readFileSync(writeFile, 'utf8');
    assert.strictEqual(content, 'hello');
    fs.unlinkSync(writeFile);
  }));
}
