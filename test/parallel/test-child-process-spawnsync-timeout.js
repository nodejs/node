'use strict';
const common = require('../common');
const assert = require('assert');

const spawnSync = require('child_process').spawnSync;

const TIMER = 200;
const SLEEP = common.platformTimeout(5000);

switch (process.argv[2]) {
  case 'child':
    setTimeout(function() {
      console.log('child fired');
      process.exit(1);
    }, SLEEP);
    break;
  default:
    const start = Date.now();
    const ret = spawnSync(process.execPath, [__filename, 'child'],
                        {timeout: TIMER});
    assert.strictEqual(ret.error.errno, 'ETIMEDOUT');
    const end = Date.now() - start;
    assert(end < SLEEP);
    assert(ret.status > 128 || ret.signal);
    break;
}
