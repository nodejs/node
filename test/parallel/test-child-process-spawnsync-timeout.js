'use strict';
require('../common');
var assert = require('assert');

var spawnSync = require('child_process').spawnSync;

var TIMER = 200;
var SLEEP = 5000;

switch (process.argv[2]) {
  case 'child':
    setTimeout(function() {
      console.log('child fired');
      process.exit(1);
    }, SLEEP);
    break;
  default:
    var start = Date.now();
    var ret = spawnSync(process.execPath, [__filename, 'child'],
                        {timeout: TIMER});
    assert.strictEqual(ret.error.errno, 'ETIMEDOUT');
    console.log(ret);
    var end = Date.now() - start;
    assert(end < SLEEP);
    assert(ret.status > 128 || ret.signal);
    break;
}
