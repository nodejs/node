// Flags: --experimental-vfs
'use strict';

// Hard-link error cases: creating a link to a directory or to an
// already-existing path.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Linking to a directory throws EINVAL
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  assert.throws(() => myVfs.linkSync('/d', '/d-link'), { code: 'EINVAL' });
}

// Linking to an existing target throws EEXIST
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'x');
  myVfs.writeFileSync('/b.txt', 'y');
  assert.throws(() => myVfs.linkSync('/a.txt', '/b.txt'), { code: 'EEXIST' });
}
