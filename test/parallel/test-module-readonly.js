'use strict';

const common = require('../common');

if (!common.isWindows) {
  // TODO: Similar checks on *nix-like systems (e.g using chmod or the like)
  common.skip('test only runs on Windows');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Create readOnlyMod.js and set to read only
const readOnlyMod = path.join(tmpdir.path, 'readOnlyMod');
const readOnlyModRelative = path.relative(__dirname, readOnlyMod);
const readOnlyModFullPath = `${readOnlyMod}.js`;

fs.writeFileSync(readOnlyModFullPath, 'module.exports = 42;');

// Removed any inherited ACEs, and any explicitly granted ACEs for the
// current user
cp.execSync(
  `icacls.exe "${readOnlyModFullPath}" /inheritance:r /remove "%USERNAME%"`);

// Grant the current user read & execute only
cp.execSync(`icacls.exe "${readOnlyModFullPath}" /grant "%USERNAME%":RX`);

let except = null;
try {
  // Attempt to load the module. Will fail if write access is required
  require(readOnlyModRelative);
} catch (err) {
  except = err;
}

// Remove the explicitly granted rights, and re-enable inheritance
cp.execSync(
  `icacls.exe "${readOnlyModFullPath}" /remove "%USERNAME%" /inheritance:e`);

// Delete the test module (note: tmpdir should get cleaned anyway)
fs.unlinkSync(readOnlyModFullPath);

assert.ifError(except);
