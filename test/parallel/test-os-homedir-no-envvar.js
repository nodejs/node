'use strict';
var common = require('../common');
var assert = require('assert');
var cp = require('child_process');
var os = require('os');
var path = require('path');


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
