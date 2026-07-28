'use strict';

// This test makes sure that `readFile()` always reads from the current
// position of the file, instead of reading from the beginning of the file.

const common = require('../common');
const assert = require('assert');
const { writeFileSync } = require('fs');
const { open } = require('fs').promises;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const fn = tmpdir.resolve('test.txt');
writeFileSync(fn, 'Hello World');

async function readFileTest() {
  const handle = await open(fn, 'r');

  /* Read only five bytes, so that the position moves to five. */
  const buf = Buffer.alloc(5);
  const { bytesRead } = await handle.read(buf, 0, 5, null);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buf.toString(), 'Hello');

  /* readFile() should read from position five, instead of zero. */
  assert.strictEqual((await handle.readFile()).toString(), ' World');

  await handle.close();
}


readFileTest()
  .then(common.mustCall());
