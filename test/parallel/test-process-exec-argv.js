'use strict';
require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.stdout.write(JSON.stringify(process.execArgv));
} else {
  const execArgv = ['--stack-size=256'];
  const args = [__filename, 'child', 'arg0'];
  const child = spawn(process.execPath, execArgv.concat(args));
  let out = '';

  child.stdout.on('data', function(chunk) {
    out += chunk;
  });

  child.on('close', function() {
    assert.deepStrictEqual(JSON.parse(out), execArgv);
  });
}
