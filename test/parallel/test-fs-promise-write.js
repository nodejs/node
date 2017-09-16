'use strict';

const common = require('../common');
const {
  async: fs
} = require('fs');
const assert = require('assert');
const {
  Buffer
} = require('buffer');
const {
  URL
} = require('url');
const path = require('path');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

const src = path.join(common.tmpDir, 'a.js');

async function test(path, data) {
  const fd = await fs.open(path, 'w+');
  try {
    await fs.write(fd, data);
    const buffer = Buffer.allocUnsafeSlow(data.length);
    await fs.read(fd, buffer);
    if (typeof data === 'string') {
      assert.strictEqual(data, buffer.toString());
    } else {
      assert.deepStrictEqual(data, buffer);
    }
  } finally {
    await fs.close(fd);
  }
}

test(src, 'testing')
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(`${src}-1`, Buffer.from('testing'))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(Buffer.from(`${src}-2`), Buffer.from('testing'))
  .then(common.mustCall())
  .catch(common.mustNotCall());

test(new URL(`file://${src}-3`), Buffer.from('testing'))
  .then(common.mustCall())
  .catch(common.mustNotCall());
