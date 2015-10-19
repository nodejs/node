'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const node = process.execPath;

if (process.argv[2] === 'child') {
  throw new Error('child error');
} else {
  run('', null);
  run('--abort-on-uncaught-exception', 'SIGABRT');
}

function run(flags, signal) {
  const args = [__filename, 'child'];
  if (flags)
    args.unshift(flags);

  const child = spawn(node, args);
  child.on('exit', common.mustCall(function(code, sig) {
    if (!common.isWindows) {
      assert.strictEqual(sig, signal);
    } else {
      if (signal)
        assert.strictEqual(code, 3);
      else
        assert.strictEqual(code, 1);
    }
  }));
}
