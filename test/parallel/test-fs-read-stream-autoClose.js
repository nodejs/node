'use strict';

const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const writeFile = tmpdir.resolve('write-autoClose.txt');
tmpdir.refresh();

const file = fs.createWriteStream(writeFile, { autoClose: true });

file.on('finish', common.mustCall(() => {
  assert.strictEqual(file.destroyed, false);
}));
file.end('asd');
