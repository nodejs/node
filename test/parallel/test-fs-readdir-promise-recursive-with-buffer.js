'use strict';

// Test that fs.promises.readdir works with Buffer paths in recursive mode.
// Refs: https://github.com/nodejs/node/issues/58892

const common = require('../common');
const tmpdir = require('../common/tmpdir');

const assert = require('node:assert');
const { readdir, mkdir, writeFile } = require('node:fs/promises');
const { join } = require('node:path');

async function runTest() {
  tmpdir.refresh();

  const subdir = join(tmpdir.path, 'subdir');
  await mkdir(subdir);
  await writeFile(join(tmpdir.path, 'file1.txt'), 'content1');
  await writeFile(join(subdir, 'file2.txt'), 'content2');

  const result1 = await readdir(Buffer.from(tmpdir.path), { recursive: true });
  assert(Array.isArray(result1));
  assert.strictEqual(result1.length, 3);

  const result2 = await readdir(Buffer.from(tmpdir.path), { recursive: true, withFileTypes: true });
  assert(Array.isArray(result2));
  assert.strictEqual(result2.length, 3);
}

runTest().then(common.mustCall());
