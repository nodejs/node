'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const os = require('os');
const path = require('path');


if (process.argv[2] === 'child') {
  if (common.isWindows)
    assert.equal(process.env.USERPROFILE, undefined);
  else
    assert.equal(process.env.HOME, undefined);

  var home = os.homedir();

  assert.ok(typeof home === 'string');
  assert.ok(home.indexOf(path.sep) !== -1);
} else {
  if (common.isWindows)
    delete process.env.USERPROFILE;
  else
    delete process.env.HOME;

  var child = cp.spawnSync(process.execPath, [__filename, 'child'], {
    env: process.env
  });

  assert.equal(child.status, 0);
}
