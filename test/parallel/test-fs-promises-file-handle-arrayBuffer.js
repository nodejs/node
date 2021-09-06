'use strict';

const common = require('../common');

// The following tests validate FileHandle.prototype.arrayBuffer method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

(async () => {
  const data = Buffer.allocUnsafe(100);
  const filePath = path.resolve(tmpDir, 'arrayBuffer');
  fs.writeFileSync(filePath, data);

  let fileHandle;
  try {
    fileHandle = await open(filePath);

    const fileContent = await fileHandle.arrayBuffer();
    assert.deepStrictEqual(Buffer.from(fileContent), data);
  } finally {
    await fileHandle?.close();
  }
})().then(common.mustCall());
