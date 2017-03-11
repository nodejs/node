'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const common = require('../common');
const msg = {test: 'this'};
const nodePath = process.execPath;
const copyPath = path.join(common.tmpDir, 'node-copy.exe');

if (process.env.FORK) {
  assert(process.send);
  assert.equal(process.argv[0], copyPath);
  process.send(msg);
  process.exit();
} else {
  common.refreshTmpDir();
  try {
    fs.unlinkSync(copyPath);
  } catch (e) {
    if (e.code !== 'ENOENT') throw e;
  }
  fs.writeFileSync(copyPath, fs.readFileSync(nodePath));
  fs.chmodSync(copyPath, '0755');

  // slow but simple
  const envCopy = JSON.parse(JSON.stringify(process.env));
  envCopy.FORK = 'true';
  const child = require('child_process').fork(__filename, {
    execPath: copyPath,
    env: envCopy
  });
  child.on('message', common.mustCall(function(recv) {
    assert.deepEqual(msg, recv);
  }));
  child.on('exit', common.mustCall(function(code) {
    fs.unlinkSync(copyPath);
    assert.equal(code, 0);
  }));
}
