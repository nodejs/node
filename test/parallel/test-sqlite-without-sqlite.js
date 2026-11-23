'use strict';

const { hasSQLite } = require('../common');
const assert = require('node:assert');

if (hasSQLite) {
  assert.strictEqual(typeof require('node:sqlite').Database, 'function');
} else {
  assert.throws(() => require('node:sqlite'), { code: 'ERR_NO_SQLITE' });
}
