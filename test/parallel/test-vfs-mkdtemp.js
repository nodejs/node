'use strict';

// mkdtemp / mkdtempSync behaviour.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// mkdtempSync returns the created directory path (with random suffix)
{
  const myVfs = vfs.create();
  const dir = myVfs.mkdtempSync('/tmp-');
  assert.ok(dir.startsWith('/tmp-'));
  assert.ok(myVfs.statSync(dir).isDirectory());
}

// mkdtemp callback variant — success
{
  const myVfs = vfs.create();
  myVfs.mkdtemp('/tmp-', common.mustCall((err, dir) => {
    assert.ifError(err);
    assert.ok(dir.startsWith('/tmp-'));
  }));
}

// mkdtemp callback variant — with options object
{
  const myVfs = vfs.create();
  myVfs.mkdtemp('/tmp-', {}, common.mustCall((err, dir) => {
    assert.ifError(err);
    assert.ok(dir.startsWith('/tmp-'));
  }));
}

// mkdtemp callback variant — error path (parent doesn't exist)
{
  const myVfs = vfs.create();
  myVfs.mkdtemp('/missing/prefix-', common.mustCall((err) => {
    assert.ok(err);
  }));
}
