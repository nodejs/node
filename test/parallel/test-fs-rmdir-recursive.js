'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const paramdir = path.join(tmpdir.path, 'dir');

// fs.rmdir - recursive: true
{
  const d = path.join(tmpdir.path, 'dir', 'test_rmdir');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));
  // Create files
  fs.writeFileSync(path.join(d, 'test.txt'), 'test');

  fs.rmdir(paramdir, { recursive: true }, common.mustCall((err) => {
    assert.ifError(err);
    assert(!fs.existsSync(d));
  }));
}

// fs.rmdirSync - recursive: true
{
  const d = path.join(tmpdir.path, 'dir', 'test_rmdirSync');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));
  // Create files
  fs.writeFileSync(path.join(d, 'test.txt'), 'test');

  fs.rmdirSync(paramdir, { recursive: true });
  assert(!fs.existsSync(d));
}

// fs.promises.rmdir - recursive: true
{
  const d = path.join(tmpdir.path, 'dir', 'test_promises_rmdir');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));
  // Create files
  fs.writeFileSync(path.join(d, 'test.txt'), 'test');

  async () => {
    await fs.promises.rmdir(paramdir, { recursive: true });
    assert(!fs.existsSync(d));
  };
}

// recursive: false
{
  const d = path.join(tmpdir.path, 'dir', 'test_rmdir_recursive_false');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));

  // fs.rmdir
  fs.rmdir(paramdir, { recursive: false }, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOTEMPTY');
  }));

  // fs.rmdirSync
  common.expectsError(
    () => fs.rmdirSync(paramdir, { recursive: false }),
    {
      code: 'ENOTEMPTY'
    }
  );

  // fs.promises.rmdir
  assert.rejects(
    fs.promises.rmdir(paramdir, { recursive: false }),
    {
      code: 'ENOTEMPTY'
    }
  );
}
