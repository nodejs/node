'use strict';

// This test makes sure that `writeFile()` always writes from the current
// position of the file, instead of truncating the file.

const common = require('../common');
const assert = require('assert');
const { readFileSync } = require('fs');
const { open } = require('fs').promises;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const fn = tmpdir.resolve('test.txt');

async function writeFileTest() {
  const handle = await open(fn, 'w');

  /* Write only five bytes, so that the position moves to five. */
  const buf = Buffer.from('Hello');
  const { bytesWritten } = await handle.write(buf, 0, 5, null);
  assert.strictEqual(bytesWritten, 5);

  /* Write some more with writeFile(). */
  await handle.writeFile('World');

  /* New content should be written at position five, instead of zero. */
  assert.strictEqual(readFileSync(fn).toString(), 'HelloWorld');

  await handle.close();
}


writeFileTest()
  .then(common.mustCall());
