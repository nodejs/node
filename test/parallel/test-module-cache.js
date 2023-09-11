'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const filePath = tmpdir.resolve('test-module-cache.json');
assert.throws(
  () => require(filePath),
  { code: 'MODULE_NOT_FOUND' }
);

fs.writeFileSync(filePath, '[]');

const content = require(filePath);
assert.strictEqual(Array.isArray(content), true);
assert.strictEqual(content.length, 0);
