// Flags: --expose_internals
'use strict';
require('../common');
const assert = require('assert');

const fs = require('fs');

const O_APPEND = fs.constants.O_APPEND || 0;
const O_CREAT = fs.constants.O_CREAT || 0;
const O_EXCL = fs.constants.O_EXCL || 0;
const O_RDONLY = fs.constants.O_RDONLY || 0;
const O_RDWR = fs.constants.O_RDWR || 0;
const O_SYNC = fs.constants.O_SYNC || 0;
const O_TRUNC = fs.constants.O_TRUNC || 0;
const O_WRONLY = fs.constants.O_WRONLY || 0;

const { stringToFlags } = require('internal/fs');

assert.strictEqual(stringToFlags('r'), O_RDONLY);
assert.strictEqual(stringToFlags('r+'), O_RDWR);
assert.strictEqual(stringToFlags('rs+'), O_RDWR | O_SYNC);
assert.strictEqual(stringToFlags('sr+'), O_RDWR | O_SYNC);
assert.strictEqual(stringToFlags('w'), O_TRUNC | O_CREAT | O_WRONLY);
assert.strictEqual(stringToFlags('w+'), O_TRUNC | O_CREAT | O_RDWR);
assert.strictEqual(stringToFlags('a'), O_APPEND | O_CREAT | O_WRONLY);
assert.strictEqual(stringToFlags('a+'), O_APPEND | O_CREAT | O_RDWR);

assert.strictEqual(stringToFlags('wx'), O_TRUNC | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(stringToFlags('xw'), O_TRUNC | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(stringToFlags('wx+'), O_TRUNC | O_CREAT | O_RDWR | O_EXCL);
assert.strictEqual(stringToFlags('xw+'), O_TRUNC | O_CREAT | O_RDWR | O_EXCL);
assert.strictEqual(stringToFlags('ax'), O_APPEND | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(stringToFlags('xa'), O_APPEND | O_CREAT | O_WRONLY | O_EXCL);
assert.strictEqual(stringToFlags('ax+'), O_APPEND | O_CREAT | O_RDWR | O_EXCL);
assert.strictEqual(stringToFlags('xa+'), O_APPEND | O_CREAT | O_RDWR | O_EXCL);

('+ +a +r +w rw wa war raw r++ a++ w++ x +x x+ rx rx+ wxx wax xwx xxx')
  .split(' ')
  .forEach(function(flags) {
    assert.throws(
      () => stringToFlags(flags),
      new RegExp(`^Error: Unknown file open flag: ${escapeRegExp(flags)}`)
    );
  });

assert.throws(
  () => stringToFlags({}),
  /^Error: Unknown file open flag: \[object Object\]$/
);

assert.throws(
  () => stringToFlags(true),
  /^Error: Unknown file open flag: true$/
);

assert.throws(
  () => stringToFlags(null),
  /^Error: Unknown file open flag: null$/
);

function escapeRegExp(string) {
  return string.replace(/[\\^$*+?.()|[\]{}]/g, '\\$&');
}
