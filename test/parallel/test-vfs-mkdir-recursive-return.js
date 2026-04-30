'use strict';

// Verify mkdirSync({ recursive: true }) returns the first directory created.
// When some parent directories already exist, the return value should be the
// first newly-created directory path.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a');

  const result = myVfs.mkdirSync('/a/b/c', { recursive: true });
  assert.strictEqual(result, '/a/b');
}
