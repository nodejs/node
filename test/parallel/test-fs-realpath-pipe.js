'use strict';

const common = require('../common');

if (common.isWindows || common.isAIX)
  common.skip(`No /dev/stdin on ${process.platform}.`);

const assert = require('assert');

const { spawnSync } = require('child_process');

for (const code of [
  `require('fs').realpath('/dev/stdin', (err, resolvedPath) => {
    if (err) {
      process.exit(1);
    }
    if (resolvedPath) {
      process.exit(2);
    }
  });`,
  `try {
    if (require('fs').realpathSync('/dev/stdin')) {
      process.exit(2);
    }
  } catch {
    process.exit(1);
  }`
]) {
  assert.strictEqual(spawnSync(process.execPath, ['-e', code], {
    stdio: 'pipe'
  }).status, 2);
}
