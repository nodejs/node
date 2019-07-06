'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const file = path.join(tmpdir.path, 'write.txt');

function checkContent(stream, expected) {
  const actual = fs.readFileSync(file, 'utf-8');
  assert.strictEqual(actual, expected);
}

{
  const stream = fs.createWriteStream(file);
  stream.write('data', common.mustCall(() => {
    stream.flush();
    checkContent(stream, 'data');
    stream.write('data');
    stream.flush(common.mustCall(() => {
      checkContent(stream, 'datadata');
      stream.destroy();
    }));
  }));
}

{
  const stream = fs.createWriteStream(file);
  stream.write('data');
  checkContent(stream, '');
  stream.flush();
  checkContent(stream, 'data');
  stream.write('data');
  stream.flush();
  checkContent(stream, 'datadata');
  stream.destroy();
}
