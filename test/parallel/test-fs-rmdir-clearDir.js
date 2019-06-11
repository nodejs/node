'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const tmpPath = (dir) => path.join(tmpdir.path, dir);

tmpdir.refresh();

// fs.rmdir - clearDir: true
{
  const paramdir = tmpPath('rmdir');
  const d = path.join(paramdir, 'test_rmdir');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));
  // Create files
  fs.writeFileSync(path.join(d, 'test.txt'), 'test');

  fs.rmdir(paramdir, { clearDir: true }, common.mustCall((err) => {
    assert.ifError(err);
    assert(!fs.existsSync(d));
  }));
}

// fs.rmdirSync - clearDir: true
{
  const paramdir = tmpPath('rmdirSync');
  const d = path.join(paramdir, 'test_rmdirSync');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));
  // Create files
  fs.writeFileSync(path.join(d, 'test.txt'), 'test');

  fs.rmdirSync(paramdir, { clearDir: true });
  assert(!fs.existsSync(d));
}

// fs.promises.rmdir - clearDir: true
{
  const paramdir = tmpPath('rmdirPromise');
  const d = path.join(paramdir, 'test_promises_rmdir');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));
  // Create files
  fs.writeFileSync(path.join(d, 'test.txt'), 'test');

  (async () => {
    await fs.promises.rmdir(paramdir, { clearDir: true });
    assert(!fs.existsSync(d));
  })();
}

// clearDir: false
{
  const paramdir = tmpPath('options');
  const d = path.join(paramdir, 'dir', 'test_rmdir_recursive_false');
  // Make sure the directory does not exist
  assert(!fs.existsSync(d));
  // Create the directory now
  fs.mkdirSync(d, { recursive: true });
  assert(fs.existsSync(d));

  // fs.rmdir
  fs.rmdir(paramdir, { clearDir: false }, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOTEMPTY');
  }));

  // fs.rmdirSync
  common.expectsError(
    () => fs.rmdirSync(paramdir, { clearDir: false }),
    {
      code: 'ENOTEMPTY'
    }
  );

  // fs.promises.rmdir
  assert.rejects(
    fs.promises.rmdir(paramdir, { clearDir: false }),
    {
      code: 'ENOTEMPTY'
    }
  );
}
