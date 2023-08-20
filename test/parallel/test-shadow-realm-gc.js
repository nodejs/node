// Flags: --experimental-shadow-realm --max-old-space-size=20
'use strict';

/**
 * Verifying ShadowRealm instances can be correctly garbage collected.
 */

const common = require('../common');
const assert = require('assert');
const { isMainThread, Worker } = require('worker_threads');

for (let i = 0; i < 100; i++) {
  const realm = new ShadowRealm();
  realm.evaluate('new TextEncoder(); 1;');
}

if (isMainThread) {
  const worker = new Worker(__filename);
  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
