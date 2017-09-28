'use strict';

const common = require('../common');
const {
  async: fs,
  readFileSync
} = require('fs');
const assert = require('assert');
const {
  Buffer
} = require('buffer');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const content = readFileSync(__filename, 'utf8');

async function test(input) {
  const fd = await fs.open(input, 'r');
  try {
    const stat = await fs.fstat(fd);
    const buffer = Buffer.allocUnsafeSlow(stat.size);
    await fs.read(fd, buffer);
    assert.strictEqual(content, buffer.toString('utf8'));
  } finally {
    await fs.close(fd);
  }
}

test(__filename)
  .then(common.mustCall())
  .catch(common.mustNotCall());
