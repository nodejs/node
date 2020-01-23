'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');

const testDir = tmpdir.path;
const files = ['empty', 'files', 'for', 'just', 'testing'];

// Make sure tmp directory is clean
tmpdir.refresh();

// Create the necessary files
files.forEach(function(filename) {
  fs.closeSync(fs.openSync(path.join(testDir, filename), 'w'));
});

function assertDirent(dirent) {
  assert(dirent instanceof fs.Dirent);
  assert.strictEqual(dirent.isFile(), true);
  assert.strictEqual(dirent.isDirectory(), false);
  assert.strictEqual(dirent.isSocket(), false);
  assert.strictEqual(dirent.isBlockDevice(), false);
  assert.strictEqual(dirent.isCharacterDevice(), false);
  assert.strictEqual(dirent.isFIFO(), false);
  assert.strictEqual(dirent.isSymbolicLink(), false);
}

const dirclosedError = {
  code: 'ERR_DIR_CLOSED'
};

const invalidCallbackObj = {
  code: 'ERR_INVALID_CALLBACK',
  name: 'TypeError'
};

// Check the opendir Sync version
{
  const dir = fs.opendirSync(testDir);
  const entries = files.map(() => {
    const dirent = dir.readSync();
    assertDirent(dirent);
    return dirent.name;
  });
  assert.deepStrictEqual(files, entries.sort());

  // dir.read should return null when no more entries exist
  assert.strictEqual(dir.readSync(), null);

  // check .path
  assert.strictEqual(dir.path, testDir);

  dir.closeSync();

  assert.throws(() => dir.readSync(), dirclosedError);
  assert.throws(() => dir.closeSync(), dirclosedError);
}

// Check the opendir async version
fs.opendir(testDir, common.mustCall(function(err, dir) {
  assert.ifError(err);
  let sync = true;
  dir.read(common.mustCall((err, dirent) => {
    assert(!sync);
    assert.ifError(err);

    // Order is operating / file system dependent
    assert(files.includes(dirent.name), `'files' should include ${dirent}`);
    assertDirent(dirent);

    let syncInner = true;
    dir.read(common.mustCall((err, dirent) => {
      assert(!syncInner);
      assert.ifError(err);

      dir.close(common.mustCall(function(err) {
        assert.ifError(err);
      }));
    }));
    syncInner = false;
  }));
  sync = false;
}));

// opendir() on file should throw ENOTDIR
assert.throws(function() {
  fs.opendirSync(__filename);
}, /Error: ENOTDIR: not a directory/);

assert.throws(function() {
  fs.opendir(__filename);
}, /TypeError \[ERR_INVALID_CALLBACK\]: Callback must be a function/);

fs.opendir(__filename, common.mustCall(function(e) {
  assert.strictEqual(e.code, 'ENOTDIR');
}));

[false, 1, [], {}, null, undefined].forEach((i) => {
  assert.throws(
    () => fs.opendir(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.opendirSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});

// Promise-based tests
async function doPromiseTest() {
  // Check the opendir Promise version
  const dir = await fs.promises.opendir(testDir);
  const entries = [];

  let i = files.length;
  while (i--) {
    const dirent = await dir.read();
    entries.push(dirent.name);
    assertDirent(dirent);
  }

  assert.deepStrictEqual(files, entries.sort());

  // dir.read should return null when no more entries exist
  assert.strictEqual(await dir.read(), null);

  await dir.close();
}
doPromiseTest().then(common.mustCall());

// Async iterator
async function doAsyncIterTest() {
  const entries = [];
  for await (const dirent of await fs.promises.opendir(testDir)) {
    entries.push(dirent.name);
    assertDirent(dirent);
  }

  assert.deepStrictEqual(files, entries.sort());

  // Automatically closed during iterator
}
doAsyncIterTest().then(common.mustCall());

// Async iterators should do automatic cleanup

async function doAsyncIterBreakTest() {
  const dir = await fs.promises.opendir(testDir);
  for await (const dirent of dir) { // eslint-disable-line no-unused-vars
    break;
  }

  await assert.rejects(async () => dir.read(), dirclosedError);
}
doAsyncIterBreakTest().then(common.mustCall());

async function doAsyncIterReturnTest() {
  const dir = await fs.promises.opendir(testDir);
  await (async function() {
    for await (const dirent of dir) { // eslint-disable-line no-unused-vars
      return;
    }
  })();

  await assert.rejects(async () => dir.read(), dirclosedError);
}
doAsyncIterReturnTest().then(common.mustCall());

async function doAsyncIterThrowTest() {
  const dir = await fs.promises.opendir(testDir);
  try {
    for await (const dirent of dir) { // eslint-disable-line no-unused-vars
      throw new Error('oh no');
    }
  } catch (err) {
    if (err.message !== 'oh no') {
      throw err;
    }
  }

  await assert.rejects(async () => dir.read(), dirclosedError);
}
doAsyncIterThrowTest().then(common.mustCall());

// Check error thrown on invalid values of bufferSize
for (const bufferSize of [-1, 0, 0.5, 1.5, Infinity, NaN]) {
  assert.throws(
    () => fs.opendirSync(testDir, { bufferSize }),
    {
      code: 'ERR_OUT_OF_RANGE'
    });
}
for (const bufferSize of ['', '1', null]) {
  assert.throws(
    () => fs.opendirSync(testDir, { bufferSize }),
    {
      code: 'ERR_INVALID_ARG_TYPE'
    });
}

// Check that passing a positive integer as bufferSize works
{
  const dir = fs.opendirSync(testDir, { bufferSize: 1024 });
  assertDirent(dir.readSync());
  dir.close();
}

// Check that when passing a string instead of function - throw an exception
async function doAsyncIterInvalidCallbackTest() {
  const dir = await fs.promises.opendir(testDir);
  assert.throws(() => dir.close('not function'), invalidCallbackObj);
}
doAsyncIterInvalidCallbackTest().then(common.mustCall());

// Check if directory already closed - throw an exception
async function doAsyncIterDirClosedTest() {
  const dir = await fs.promises.opendir(testDir);
  await dir.close();

  assert.throws(() => dir.close(), dirclosedError);
}
doAsyncIterDirClosedTest().then(common.mustCall());
