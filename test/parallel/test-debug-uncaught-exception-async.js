'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const spawn = require('child_process').spawn;

const emitUncaught = path.join(common.fixturesDir, 'debug-uncaught-async.js');
const result = spawn(process.execPath, [emitUncaught], {encoding: 'utf8'});

var stderr = '';
result.stderr.on('data', (data) => {
  stderr += data;
});

result.on('close', (code) => {
  const expectedMessage =
    "There was an internal error in Node's debugger. Please report this bug.";

  assert.strictEqual(code, 1);
  assert(stderr.includes(expectedMessage));
  assert(stderr.includes('Error: fhqwhgads'));
});
