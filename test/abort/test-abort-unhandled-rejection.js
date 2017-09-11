'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const node = process.execPath;

if (process.argv[2] === 'child') {
  Promise.reject(new Error('child error'));
} else {
  run('', null);
  if (!common.isWindows) {
    run('--abort-on-unhandled-rejection', ['SIGABRT', 'SIGTRAP', 'SIGILL']);
    run('--abort_on_unhandled_rejection', ['SIGABRT', 'SIGTRAP', 'SIGILL']);
  }
}

function run(flags, signals) {
  const args = [__filename, 'child'];
  if (flags)
    args.unshift(flags);

  const child = spawn(node, args);
  child.on('exit', common.mustCall(function(code, sig) {
    if (signals)
      assert(signals.includes(sig), `Unexpected signal ${sig}`);
    else
      assert.strictEqual(sig, null);
  }));
}
