// Flags: --expose-internals
'use strict';

const common = require('../common');

const assert = require('assert');
const { writeFile, readFile } = require('fs').promises;
const tmpdir = require('../common/tmpdir');
const { internalBinding } = require('internal/test/binding');
const fsBinding = internalBinding('fs');
tmpdir.refresh();

const fn = tmpdir.resolve('large-file');

// Creating large buffer with random content
const largeBuffer = Buffer.from(
  Array.from({ length: 1024 ** 2 + 19 }, (_, index) => index)
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
  }).then(common.mustCall());
}

function validateReadFileAbortLogicDuring() {
  const controller = new AbortController();
  const signal = controller.signal;
  process.nextTick(() => controller.abort());
  assert.rejects(readFile(fn, { signal }), {
    name: 'AbortError'
  }).then(common.mustCall());
}

async function validateWrongSignalParam() {
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown

  await assert.rejects(async () => {
    const callback = common.mustNotCall();
    await readFile(fn, { signal: 'hello' }, callback);
  }, { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });

}

async function validateZeroByteLiar() {
  const originalFStat = fsBinding.fstat;
  fsBinding.fstat = common.mustCall(
    async () => (/* stat fields */ [0, 1, 2, 3, 4, 5, 6, 7, 0 /* size */])
  );
  const readBuffer = await readFile(fn);
  assert.strictEqual(readBuffer.toString(), largeBuffer.toString());
  fsBinding.fstat = originalFStat;
}

(async () => {
  await createLargeFile();
  await validateReadFile();
  await validateReadFileProc();
  await validateReadFileAbortLogicBefore();
  await validateReadFileAbortLogicDuring();
  await validateWrongSignalParam();
  await validateZeroByteLiar();
})().then(common.mustCall());
