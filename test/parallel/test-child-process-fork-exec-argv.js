'use strict';
require('../common');
const assert = require('assert');
const child_process = require('child_process');
const spawn = child_process.spawn;
const fork = child_process.fork;

if (process.argv[2] === 'fork') {
  process.stdout.write(JSON.stringify(process.execArgv), function() {
    process.exit();
  });
} else if (process.argv[2] === 'child') {
  fork(__filename, ['fork']);
} else {
  const execArgv = ['--stack-size=256'];
  const args = [__filename, 'child', 'arg0'];

  const child = spawn(process.execPath, execArgv.concat(args));
  let out = '';

  child.stdout.on('data', function(chunk) {
    out += chunk;
  });

  child.on('exit', function() {
    assert.deepStrictEqual(JSON.parse(out), execArgv);
  });
}
