'use strict';

// Test that 'close' emits once and not twice when `emitClose: true` is set.
// Refs: https://github.com/nodejs/node/issues/31366

const common = require('../common');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filepath = path.join(tmpdir.path, 'write_pos.txt');

const fileReadStream = fs.createReadStream(process.execPath);
const fileWriteStream = fs.createWriteStream(filepath, {
  emitClose: true
});

fileReadStream.pipe(fileWriteStream);
fileWriteStream.on('close', common.mustCall());
