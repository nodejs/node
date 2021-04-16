'use strict';
// Flags: --expose-internals

const common = require('../common');
const tmpdir = require('../common/tmpdir');

// The following tests validate aggregate errors are thrown correctly
// when both an operation and close throw.

const fs = require('fs');
const path = require('path');
const {
  readFile,
  writeFile,
  truncate,
  lchmod,
} = fs.promises;
const {
  FileHandle,
} = require('internal/fs/promises');

const assert = require('assert');
const originalFd = Object.getOwnPropertyDescriptor(FileHandle.prototype, 'fd');

let count = 0;
async function createFile() {
  const filePath = path.join(tmpdir.path, `close_errors_${++count}.txt`);
  await writeFile(filePath, 'content');
  return filePath;
}

async function checkAggregateError(op) {
  try {
    const filePath = await createFile();
    Object.defineProperty(FileHandle.prototype, 'fd', {
      get: function() {
        // Close is set by using a setter,
        // so it needs to be set on the instance.
        const originalClose = this.close;
        this.close = async () => {
          // close the file
          await originalClose.call(this);
          const closeError = new Error('CLOSE_ERROR');
          closeError.code = 456;
          throw closeError;
        };
        return originalFd.get.call(this);
      }
    });

    await op(filePath).catch(common.mustCall((err) => {
      assert.strictEqual(err.constructor.name, 'Error');
      assert.strictEqual(err.message, 'CLOSE_ERROR');
      assert.strictEqual(err.code, 456);
    }));
  } finally {
    Object.defineProperty(FileHandle.prototype, 'fd', originalFd);
  }
}
(async function() {
  tmpdir.refresh();
  await checkAggregateError((filePath) => truncate(filePath));
  await checkAggregateError((filePath) => readFile(filePath));
  await checkAggregateError((filePath) => writeFile(filePath, '123'));
  if (common.isOSX) {
    await checkAggregateError((filePath) => lchmod(filePath, 0o777));
  }
})().then(common.mustCall());
