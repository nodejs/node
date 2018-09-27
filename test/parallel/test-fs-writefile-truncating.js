'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const fsPromises = require('fs').promises;
const tmpdir = require('../common/tmpdir');
const tmpDir = tmpdir.path;
tmpdir.refresh();

// Length of First should be greater than the length of Second. When
// the file is written the second time, it should clear out all the
// contents of the file instead of starting at the end of the file or
// at the beginning of the file replacing characters in the existing
// content.
const First = 'Callbacks';
const Second = 'Promises';

{
  const fileName = path.resolve(tmpDir, 'first.txt');
  fs.writeFileSync(fileName, First);
  fs.writeFileSync(fileName, Second);
  assert.deepStrictEqual(fs.readFileSync(fileName, 'utf8'), Second);
}

{
  const fileName = path.resolve(tmpDir, 'second.txt');
  const fd = fs.openSync(fileName, 'w');
  fs.writeFileSync(fd, First);
  fs.writeFileSync(fd, Second);
  fs.closeSync(fd);
  assert.deepStrictEqual(fs.readFileSync(fileName, 'utf8'), Second);
}

(async function() {
  const fileName = path.resolve(tmpDir, 'third.txt');
  await fsPromises.writeFile(fileName, First);
  await fsPromises.writeFile(fileName, Second);
  assert.deepStrictEqual(fs.readFileSync(fileName, 'utf8'), Second);
})().then(common.mustCall());

(async function() {
  const fileName = path.resolve(tmpDir, 'fourth.txt');
  const fileHandle = await fsPromises.open(fileName, 'w');
  await fileHandle.writeFile(First);
  await fileHandle.writeFile(Second);
  await fileHandle.close();
  assert.deepStrictEqual(fs.readFileSync(fileName, 'utf8'), Second);
})().then(common.mustCall());

{
  const fileName = path.resolve(tmpDir, 'fifth.txt');
  fs.writeFile(fileName, First, common.mustCall(function(err) {
    assert.ifError(err);
    fs.writeFile(fileName, Second, common.mustCall(function(err) {
      assert.ifError(err);
      assert.deepStrictEqual(fs.readFileSync(fileName, 'utf8'), Second);
    }));
  }));
}

{
  const fileName = path.resolve(tmpDir, 'sixth.txt');
  fs.open(fileName, 'w', common.mustCall(function(err, handle) {
    assert.ifError(err);
    fs.writeFile(handle, First, common.mustCall(function(err) {
      assert.ifError(err);
      fs.writeFile(handle, Second, common.mustCall(function(err) {
        assert.ifError(err);
        fs.closeSync(handle);
        assert.deepStrictEqual(fs.readFileSync(fileName, 'utf8'), Second);
      }));
    }));
  }));
}
