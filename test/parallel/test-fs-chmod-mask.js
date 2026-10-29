'use strict';

// This tests that the lower bits of mode > 0o777 still works in fs APIs.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

let mode;
// On Windows chmod is only able to manipulate write permission
if (common.isWindows) {
  mode = 0o444;  // read-only
} else {
  mode = 0o777;
}

const maskToIgnore = 0o10000;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function test(mode, asString) {
  const suffix = asString ? 'str' : 'num';
  const input = asString ?
    (mode | maskToIgnore).toString(8) : (mode | maskToIgnore);

  {
    const file = tmpdir.resolve(`chmod-async-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');

    fs.chmod(file, input, common.mustSucceed(() => {
      assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
    }));
  }

  {
    const file = tmpdir.resolve(`chmodSync-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');

    fs.chmodSync(file, input);
    assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
  }

  {
    const file = tmpdir.resolve(`fchmod-async-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');
    fs.open(file, 'w', common.mustSucceed((fd) => {
      fs.fchmod(fd, input, common.mustSucceed(() => {
        assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);
        fs.close(fd, assert.ifError);
      }));
    }));
  }

  {
    const file = tmpdir.resolve(`fchmodSync-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');
    const fd = fs.openSync(file, 'w');

    fs.fchmodSync(fd, input);
    assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);

    fs.close(fd, assert.ifError);
  }

  if (fs.lchmod) {
    const link = tmpdir.resolve(`lchmod-src-${suffix}`);
    const file = tmpdir.resolve(`lchmod-dest-${suffix}`);
    fs.writeFileSync(file, 'test', 'utf-8');
    fs.symlinkSync(file, link);

    fs.lchmod(link, input, common.mustSucceed(() => {
      assert.strictEqual(fs.lstatSync(link).mode & 0o777, mode);
    }));
  }

  if (fs.lchmodSync) {
    const link = tmpdir.resolve(`lchmodSync-src-${suffix}`);
    const file = tmpdir.resolve(`lchmodSync-dest-${suffix}`);
    fs.writeFileSync(file, 'test', 'utf-8');
    fs.symlinkSync(file, link);

    fs.lchmodSync(link, input);
    assert.strictEqual(fs.lstatSync(link).mode & 0o777, mode);
  }
}

test(mode, true);
test(mode, false);
