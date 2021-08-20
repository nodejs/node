'use strict';

const common = require('../common');

const assert = require('assert');
const path = require('path');
const { writeFile, readFile } = require('fs').promises;
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const fn = path.join(tmpdir.path, 'large-file');

// Creating large buffer with random content
const largeBuffer = Buffer.from(
  Array.apply(null, { length: 16834 * 2 })
    .map(Math.random)
    .map((number) => (number * (1 << 8)))
);

async function createLargeFile() {
  // Writing buffer to a file then try to read it
  await writeFile(fn, largeBuffer);
}

async function validateReadFile() {
  const readBuffer = await readFile(fn);
  assert.strictEqual(readBuffer.equals(largeBuffer), true);
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

function validateReadFileAbortLogicBefore() {
  const signal = AbortSignal.abort();
  assert.rejects(readFile(fn, { signal }), {
    name: 'AbortError'
  });
}

function validateReadFileAbortLogicDuring() {
  const controller = new AbortController();
  const signal = controller.signal;
  process.nextTick(() => controller.abort());
  assert.rejects(readFile(fn, { signal }), {
    name: 'AbortError'
  });
}

async function validateWrongSignalParam() {
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown

  await assert.rejects(async () => {
    const callback = common.mustNotCall(() => {});
    await readFile(fn, { signal: 'hello' }, callback);
  }, { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });

}

(async () => {
  await createLargeFile();
  await validateReadFile();
  await validateReadFileProc();
  await validateReadFileAbortLogicBefore();
  await validateReadFileAbortLogicDuring();
  await validateWrongSignalParam();
})().then(common.mustCall());
