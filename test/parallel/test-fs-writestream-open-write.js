'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { strictEqual } = require('assert');
const { join } = require('path');
const fs = require('fs');

// Regression test for https://github.com/nodejs/node/issues/51993

tmpdir.refresh();

const file = join(tmpdir.path, `test-fs-writestream-open-write.txt`);

const w = fs.createWriteStream(file);

w.on('open', common.mustCall(() => {
  w.write('hello');

  process.nextTick(() => {
    w.write('world');
    w.end();
  });
}));

w.on('close', common.mustCall(() => {
  strictEqual(fs.readFileSync(file, 'utf8'), 'helloworld');
  fs.unlinkSync(file);
}));
