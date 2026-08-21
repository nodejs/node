'use strict';
const common = require('../common');
const assert = require('assert');
const os = require('os');
const path = require('path');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');


if (process.argv[2] === 'child') {
  if (common.isWindows)
    assert.strictEqual(process.env.USERPROFILE, undefined);
  else
    assert.strictEqual(process.env.HOME, undefined);

  const home = os.homedir();

  assert.strictEqual(typeof home, 'string');
  assert(home.includes(path.sep));
} else {
  if (common.isWindows)
    delete process.env.USERPROFILE;
  else
    delete process.env.HOME;

  spawnSyncAndExitWithoutError(process.execPath, [__filename, 'child'], {
    env: process.env
  });
}
