'use strict';

// This tests that the lower bits of mode > 0o777 still works in fs.open().

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const mode = common.isWindows ? 0o444 : 0o644;

const maskToIgnore = 0o10000;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function test(mode, asString) {
  const suffix = asString ? 'str' : 'num';
  const input = asString ?
    (mode | maskToIgnore).toString(8) : (mode | maskToIgnore);

  {
    const file = tmpdir.resolve(`openSync-${suffix}.txt`);
    const fd = fs.openSync(file, 'w+', input);
    assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);
    fs.closeSync(fd);
    assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
  }

  {
    const file = tmpdir.resolve(`open-${suffix}.txt`);
    fs.open(file, 'w+', input, common.mustSucceed((fd) => {
      assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);
      fs.closeSync(fd);
      assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
    }));
  }
}

test(mode, true);
test(mode, false);
