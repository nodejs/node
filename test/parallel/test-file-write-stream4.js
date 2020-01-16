'use strict';
const common = require('../common');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

const filepath = path.join(tmpdir.path, 'write_pos.txt');
fs.mkdirSync(tmpdir.path, { recursive: true });

const fileReadStream = fs.createReadStream(process.execPath);
const fileWriteStream = fs.createWriteStream(filepath, {
  emitClose: true
});

fileReadStream.pipe(fileWriteStream);
fileWriteStream.on('close', common.mustCall());
