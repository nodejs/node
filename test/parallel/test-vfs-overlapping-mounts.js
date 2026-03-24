'use strict';

// Overlapping VFS mounts must be rejected.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const vfs1 = vfs.create();
vfs1.writeFileSync('/a.txt', 'a');
vfs1.mount('/overlap_base');

// Child of existing mount
{
  const v = vfs.create();
  v.writeFileSync('/b.txt', 'b');
  assert.throws(() => v.mount('/overlap_base/sub'), {
    code: 'ERR_INVALID_STATE',
    message: /overlaps/,
  });
}

// Non-overlapping prefix (not a parent directory)
{
  const v = vfs.create();
  v.writeFileSync('/b.txt', 'b');
  v.mount('/overlap');
  v.unmount();
}

// Parent of existing mount
{
  const v = vfs.create();
  v.writeFileSync('/b.txt', 'b');
  assert.throws(() => v.mount('/'), {
    code: 'ERR_INVALID_STATE',
    message: /overlaps/,
  });
}

// Exact same mount point
{
  const v = vfs.create();
  v.writeFileSync('/b.txt', 'b');
  assert.throws(() => v.mount('/overlap_base'), {
    code: 'ERR_INVALID_STATE',
    message: /overlaps/,
  });
}

vfs1.unmount();
