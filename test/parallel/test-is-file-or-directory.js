'use strict';

require('../common');
const fs = require('fs');
const assert = require('assert');

assert.strictEqual(typeof fs.isFileSync, 'function');
assert.strictEqual(typeof fs.isDirectorySync, 'function');

{
  assert.strictEqual(fs.isFileSync(__filename), true);
  assert.strictEqual(fs.isDirectorySync(__filename), false);
}

{
  assert.strictEqual(fs.isFileSync('./thisdoesntexist.txt'), false);
  assert.strictEqual(fs.isDirectorySync('./thisdirectorydoestexist'), false);
}
