'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Tests that process.mainArgs returns all arguments provided
if (process.mainArgs[0] === 'child') {
  console.log(process.mainArgs.join(' '));
  return;
}

const child = spawnSync(process.execPath, [__filename, 'child', '-a', 'one']);
assert.strictEqual(child.stdout.toString().trim(), 'child -a one');
