// Flags: --expose-gc --no-warnings --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = process.binding('fs');
const { stringToFlags } = require('internal/fs');

// Verifies that the FileHandle object is garbage collected and that a
// warning is emitted if it is not closed.

let fdnum;
{
  const ctx = {};
  fdnum = fs.openFileHandle(path.toNamespacedPath(__filename),
                            stringToFlags('r'), 0o666, undefined, ctx).fd;
  assert.strictEqual(ctx.errno, undefined);
}

common.expectWarning(
  'Warning',
  `Closing file descriptor ${fdnum} on garbage collection`
);

gc();  // eslint-disable-line no-undef

setTimeout(() => {}, 10);
