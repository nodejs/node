'use strict';

// This tests that reading a directory with the largest bufferSize allowed
// either works or fails with ERR_MEMORY_ALLOCATION_FAILED, instead of
// aborting the process, and that the Dir can still be closed afterwards.

const common = require('../common');

// On Windows, libuv zeroes the whole entry buffer on every read, so an
// allocation that succeeds there would write all of it.
if (common.isWindows)
  common.skip('libuv zeroes the whole readdir buffer on Windows');
if (common.isASan)
  common.skip('ASan aborts on allocations it cannot satisfy');

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
fs.writeFileSync(tmpdir.resolve('file'), '');

const bufferSize = 2 ** 32 - 1;

// Whether the buffer can be allocated depends on the memory of the machine.
function check(err, names) {
  if (err) {
    assert.strictEqual(err.code, 'ERR_MEMORY_ALLOCATION_FAILED');
  } else {
    assert.deepStrictEqual(names, ['file']);
  }
}

{
  const dir = fs.opendirSync(tmpdir.path, { bufferSize });
  let err;
  let names;
  try {
    names = [dir.readSync().name];
  } catch (e) {
    err = e;
  }
  check(err, names);
  dir.closeSync();
}

fs.opendir(tmpdir.path, { bufferSize }, common.mustSucceed((dir) => {
  dir.read(common.mustCall((err, dirent) => {
    check(err, [dirent?.name]);
    dir.close(common.mustSucceed());
  }));
}));

(async () => {
  const dir = await fs.promises.opendir(tmpdir.path, { bufferSize });
  const names = [];
  let err;
  try {
    for await (const dirent of dir) names.push(dirent.name);
  } catch (e) {
    err = e;
  }
  check(err, names);
})().then(common.mustCall());
