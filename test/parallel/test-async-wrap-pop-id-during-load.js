'use strict';

require('../common');

if (process.argv[2] === 'async') {
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
  ['--unhandled-rejections=none', '--stack_size=150', __filename, 'async'],
  { maxBuffer: Infinity }
);
assert.strictEqual(ret.status, 0,
                   `EXIT CODE: ${ret.status}, STDERR:\n${ret.stderr}`);
const stderr = ret.stderr.toString('utf8', 0, 2048);
assert.doesNotMatch(stderr, /async.*hook/i);
assert.ok(stderr.includes('Maximum call stack size exceeded'), stderr);
