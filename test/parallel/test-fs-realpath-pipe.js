'use strict';

const common = require('../common');

if (common.isWindows || common.isAIX || common.isIBMi)
  common.skip(`No /dev/stdin on ${process.platform}.`);

const assert = require('assert');

const { spawnSync } = require('child_process');

for (const code of [
  `require('fs').realpath('/dev/stdin', (err, resolvedPath) => {
    if (err) {
      console.error(err);
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
  } catch (e) {
    console.error(e);
    process.exit(1);
  }`,
]) {
  const child = spawnSync(process.execPath, ['-e', code], {
    stdio: 'pipe'
  });
  if (child.status !== 2) {
    console.log(code);
    console.log(child.stderr.toString());
  }
  assert.strictEqual(child.status, 2);
}
