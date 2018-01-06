'use strict';

require('../common');

if (process.argv[2] === 'async') {
  async function fn() {
    fn();
    throw new Error();
  }
  (async function() { await fn(); })();
  // While the above should error, just in case it doesn't the script shouldn't
  // fork itself indefinitely so return early.
  return;
}

const assert = require('assert');
const { spawnSync } = require('child_process');

const ret = spawnSync(process.execPath, [__filename, 'async']);
assert.strictEqual(ret.status, 0);
assert.ok(!/async.*hook/i.test(ret.stderr.toString('utf8', 0, 1024)));
