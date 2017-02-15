'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const spawnSync = require('child_process').spawnSync;

const emitUncaught = path.join(common.fixturesDir, 'debug-uncaught.js');
const result = spawnSync(process.execPath, [emitUncaught], {encoding: 'utf8'});

const expectedMessage =
  "There was an internal error in Node's debugger. Please report this bug.";

assert.strictEqual(result.status, 1);
assert(result.stderr.includes(expectedMessage));
assert(result.stderr.includes('Error: sterrance'));

console.log(result.stdout);
