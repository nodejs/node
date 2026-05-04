// Flags: --experimental-vfs
'use strict';

// mkdirSync / rmdirSync behaviour: return value, recursive option, mode
// option, error cases.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// mkdirSync({ recursive: true }) returns the path of the first newly-
// created directory when some parents already exist.
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a');
  const result = myVfs.mkdirSync('/a/b/c', { recursive: true });
  assert.strictEqual(result, '/a/b');
}

// mkdirSync with explicit mode (non-recursive)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d-mode', { mode: 0o700 });
  assert.strictEqual(myVfs.statSync('/d-mode').mode & 0o777, 0o700);
}

// mkdirSync with explicit mode + recursive
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/r-mode/sub/deep', { recursive: true, mode: 0o700 });
  assert.strictEqual(myVfs.statSync('/r-mode/sub/deep').mode & 0o777, 0o700);
}

// Recursive mkdir through a regular-file blocker throws ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/blocker', 'x');
  assert.throws(
    () => myVfs.mkdirSync('/blocker/sub', { recursive: true }),
    { code: 'ENOTDIR' });
}

// Rmdir on a non-empty directory throws ENOTEMPTY
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/d');
  myVfs.writeFileSync('/d/x', '');
  assert.throws(() => myVfs.rmdirSync('/d'), { code: 'ENOTEMPTY' });
}
