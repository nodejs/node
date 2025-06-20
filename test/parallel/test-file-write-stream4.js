// Flags: --expose-gc
'use strict';

// Test that 'close' emits once and not twice when `emitClose: true` is set.
// Refs: https://github.com/nodejs/node/issues/31366

const common = require('../common');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filepath = tmpdir.resolve('write_pos.txt');

const fileReadStream = fs.createReadStream(process.execPath);
const fileWriteStream = fs.createWriteStream(filepath, {
  emitClose: true
});

fileReadStream.pipe(fileWriteStream);
fileWriteStream.on('close', common.mustCall(() => {
  // TODO(lpinca): Remove the forced GC when
  // https://github.com/nodejs/node/issues/54918 is fixed.
  globalThis.gc({ type: 'major' });
}));
