'use strict';

const common = require('../common');

const assert = require('assert');
const path = require('path');
const { writeFile, readFile } = require('fs').promises;
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const fn = path.join(tmpdir.path, 'large-file');

async function validateReadFile() {
  // Creating large buffer with random content
  const buffer = Buffer.from(
    Array.apply(null, { length: 16834 * 2 })
      .map(Math.random)
      .map((number) => (number * (1 << 8)))
  );

  // Writing buffer to a file then try to read it
  await writeFile(fn, buffer);
  const readBuffer = await readFile(fn);
  assert.strictEqual(readBuffer.equals(buffer), true);
}

async function validateReadFileProc() {
  // Test to make sure reading a file under the /proc directory works. Adapted
  // from test-fs-read-file-sync-hostname.js.
  // Refs:
  // - https://groups.google.com/forum/#!topic/nodejs-dev/rxZ_RoH1Gn0
  // - https://github.com/nodejs/node/issues/21331

  // Test is Linux-specific.
  if (!common.isLinux)
    return;

  const hostname = await readFile('/proc/sys/kernel/hostname');
  assert.ok(hostname.length > 0);
}

validateReadFile()
  .then(() => validateReadFileProc())
  .then(common.mustCall());
