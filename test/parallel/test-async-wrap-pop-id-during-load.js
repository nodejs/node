'use strict';

const common = require('../common');

if (process.argv[2] === 'async') {
  common.disableCrashOnUnhandledRejection();
  async function fn() {
    fn();
    throw new Error();
  }
  return (async function() { await fn(); })();
}

const assert = require('assert');
const { spawnSync } = require('child_process');

const ret = spawnSync(
  process.execPath,
  ['--stack_size=150', __filename, 'async']
);
assert.strictEqual(ret.status, 0,
                   `EXIT CODE: ${ret.status}, STDERR:\n${ret.stderr}`);
const stderr = ret.stderr.toString('utf8', 0, 2048);
assert.ok(!/async.*hook/i.test(stderr));
assert.ok(stderr.includes('UnhandledPromiseRejectionWarning: Error'), stderr);
