'use strict';
require('../common');
const path = require('path');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] !== 'child') {
  const child = spawn(process.execPath, [__filename, 'child'], {
    cwd: path.dirname(process.execPath)
  });

  let childArgv0 = '';
  child.stdout.on('data', function(chunk) {
    childArgv0 += chunk;
  });
  process.on('exit', function() {
    assert.strictEqual(childArgv0, process.execPath);
  });
} else {
  process.stdout.write(process.argv[0]);
}
