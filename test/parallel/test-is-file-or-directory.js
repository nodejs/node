'use strict';

require('../common');
const fs = require('fs');
const assert = require('assert');

assert.strictEqual(typeof fs.isFile, 'function');
assert.strictEqual(typeof fs.isDirectory, 'function');

{
  assert.strictEqual(fs.isFile(__filename), true);
  assert.strictEqual(fs.isDirectory(__filename), false);
}

{
  assert.strictEqual(fs.isFile('./thisdoesntexist.txt'), false);
  assert.strictEqual(fs.isDirectory('./thisdirectorydoestexist'), false);
}
