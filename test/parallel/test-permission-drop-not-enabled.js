'use strict';

require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  process.exit(0);
}

const assert = require('assert');

{
  assert.strictEqual(process.permission, undefined);
}
