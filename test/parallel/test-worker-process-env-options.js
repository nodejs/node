'use strict';
require('../common');
const { Worker } = require('node:worker_threads');

if (!require.main) {
  globalThis.setup = true;
} else {
  process.env.NODE_OPTIONS ??= '';
  process.env.NODE_OPTIONS += ` --require ${JSON.stringify(__filename)}`;

  new Worker(`
    const assert = require('assert');
    assert.strictEqual(globalThis.setup, true);
  `, { eval: true });
}
