'use strict';

const common = require('../common');
const fs = require('fs');
const fsPromises = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

const dest = path.resolve(tmpDir, 'tmp.txt');
// Use a file size larger than `kReadFileMaxChunkSize`.
const buffer = Buffer.from('012'.repeat(2 ** 14));

(async () => {
  for (const Constructor of [Uint8Array, Uint16Array, Uint32Array]) {
    const array = new Constructor(buffer.buffer);
    await fsPromises.writeFile(dest, array);
    const data = await fsPromises.readFile(dest);
    assert.deepStrictEqual(data, buffer);
  }
})().then(common.mustCall());
