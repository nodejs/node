'use strict';

// This tests that the lower bits of mode > 0o777 still works in fs APIs.

const common = require('../common');
const assert = require('assert');
const path = require('path');
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
    const file = path.join(tmpdir.path, `chmod-async-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');

    fs.chmod(file, input, common.mustCall((err) => {
      assert.ifError(err);
      assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
    }));
  }

  {
    const file = path.join(tmpdir.path, `chmodSync-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');

    fs.chmodSync(file, input);
    assert.strictEqual(fs.statSync(file).mode & 0o777, mode);
  }

  {
    const file = path.join(tmpdir.path, `fchmod-async-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');
    fs.open(file, 'w', common.mustCall((err, fd) => {
      assert.ifError(err);

      fs.fchmod(fd, input, common.mustCall((err) => {
        assert.ifError(err);
        assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);
        fs.close(fd, assert.ifError);
      }));
    }));
  }

  {
    const file = path.join(tmpdir.path, `fchmodSync-${suffix}.txt`);
    fs.writeFileSync(file, 'test', 'utf-8');
    const fd = fs.openSync(file, 'w');

    fs.fchmodSync(fd, input);
    assert.strictEqual(fs.fstatSync(fd).mode & 0o777, mode);

    fs.close(fd, assert.ifError);
  }

  if (fs.lchmod) {
    const link = path.join(tmpdir.path, `lchmod-src-${suffix}`);
    const file = path.join(tmpdir.path, `lchmod-dest-${suffix}`);
    fs.writeFileSync(file, 'test', 'utf-8');
    fs.symlinkSync(file, link);

    fs.lchmod(link, input, common.mustCall((err) => {
      assert.ifError(err);
      assert.strictEqual(fs.lstatSync(link).mode & 0o777, mode);
    }));
  }

  if (fs.lchmodSync) {
    const link = path.join(tmpdir.path, `lchmodSync-src-${suffix}`);
    const file = path.join(tmpdir.path, `lchmodSync-dest-${suffix}`);
    fs.writeFileSync(file, 'test', 'utf-8');
    fs.symlinkSync(file, link);

    fs.lchmodSync(link, input);
    assert.strictEqual(fs.lstatSync(link).mode & 0o777, mode);
  }
}

test(mode, true);
test(mode, false);
