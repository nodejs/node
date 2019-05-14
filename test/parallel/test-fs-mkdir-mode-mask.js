'use strict';

// This tests that the lower bits of mode > 0o777 still works in fs.mkdir().

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

let mode;

if (common.isWindows) {
  common.skip('mode is not supported in mkdir on Windows');
  return;
} else {
  mode = 0o644;
}

const maskToIgnore = 0o10000;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function test(mode, asString) {
  const suffix = asString ? 'str' : 'num';
  const input = asString ?
    (mode | maskToIgnore).toString(8) : (mode | maskToIgnore);

  {
    const dir = path.join(tmpdir.path, `mkdirSync-${suffix}`);
    fs.mkdirSync(dir, input);
    assert.strictEqual(fs.statSync(dir).mode & 0o777, mode);
  }

  {
    const dir = path.join(tmpdir.path, `mkdir-${suffix}`);
    fs.mkdir(dir, input, common.mustCall((err) => {
      assert.ifError(err);
      assert.strictEqual(fs.statSync(dir).mode & 0o777, mode);
    }));
  }
}

test(mode, true);
test(mode, false);
