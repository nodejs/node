'use strict';

const common = require('../common');

// The following tests validate FileHandle.prototype.text method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

(async () => {
  const data = 'Hello World!';
  const filePath = path.resolve(tmpDir, 'text');
  fs.writeFileSync(filePath, data);

  let fileHandle;
  try {
    fileHandle = await open(filePath);

    const fileContent = await fileHandle.text();
    assert.strictEqual(fileContent, data);
  } finally {
    await fileHandle?.close();
  }
})().then(common.mustCall());
