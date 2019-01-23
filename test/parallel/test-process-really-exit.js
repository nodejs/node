'use strict';
require('../common');
const assert = require('assert');

// ensure that the reallyExit hook is executed.
// see: https://github.com/nodejs/node/issues/25650
if (process.argv[2] === 'subprocess') {
  process.reallyExit = function() {
    console.info('really exited');
  };
  process.exit();
} else {
  const { spawnSync } = require('child_process');
  const out = spawnSync(process.execPath, [__filename, 'subprocess']);
  const observed = out.output[1].toString('utf8').trim();
  assert.strictEqual(observed, 'really exited');
}
