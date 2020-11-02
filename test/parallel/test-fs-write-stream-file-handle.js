'use strict';
const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const file = path.join(tmpdir.path, 'write_stream_filehandle_test.txt');
const input = 'hello world';

tmpdir.refresh();

fs.promises.open(file, 'w+').then(common.mustCall((handle) => {
  handle.on('close', common.mustCall());
  const stream = fs.createWriteStream(null, { fd: handle });

  stream.end(input);
  stream.on('close', common.mustCall(() => {
    const output = fs.readFileSync(file, 'utf-8');
    assert.strictEqual(output, input);
  }));
}));
