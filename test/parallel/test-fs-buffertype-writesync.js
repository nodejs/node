'use strict';
require('../common');

// This test ensures that writeSync does support inputs which
// are then correctly converted into string buffers.

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');

const filePath = path.join(tmpdir.path, 'test_buffer_type');
const v = [true, false, 0, 1, Infinity, () => {}, {}, [], undefined, null];

tmpdir.refresh();

v.forEach((value) => {
  const fd = fs.openSync(filePath, 'w');
  fs.writeSync(fd, value);
  assert.strictEqual(fs.readFileSync(filePath).toString(), String(value));
});
