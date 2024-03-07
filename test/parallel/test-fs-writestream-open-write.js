'use strict';

const common = require('../common');
const { strictEqual } = require('assert');
const { tmpdir } = require('os');
const { join } = require('path');
const fs = require('fs');

// Regression test for https://github.com/nodejs/node/issues/51993

const file = join(tmpdir(), `test-fs-writestream-open-write-${process.pid}.txt`);

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
