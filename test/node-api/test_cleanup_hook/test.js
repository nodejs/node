'use strict';
const common = require('../../common');
const assert = require('assert');
const child_process = require('child_process');

if (process.argv[2] === 'child') {
  require(`./build/${common.buildType}/binding`);
} else {
  const { stdout, status, signal } =
    child_process.spawnSync(process.execPath, [__filename, 'child']);
  assert.strictEqual(status, 0, `process exited with status(${status}) and signal(${signal})`);
  assert.strictEqual(stdout.toString().trim(), 'cleanup(42)');
}
