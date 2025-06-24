'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const vm = require('vm');
const node = process.execPath;


if (process.argv[2] === 'child') {
  throw new Error('child error');
} else if (process.argv[2] === 'vm') {
  // Refs: https://github.com/nodejs/node/issues/13258
  // This *should* still crash.
  new vm.Script('[', {});
} else {
  run('', 'child', null);
  run('--abort-on-uncaught-exception', 'child',
      ['SIGABRT', 'SIGTRAP', 'SIGILL']);
  run('--abort-on-uncaught-exception', 'vm', ['SIGABRT', 'SIGTRAP', 'SIGILL']);
}

function run(flags, argv2, signals) {
  const args = [__filename, argv2];
  if (flags)
    args.unshift(flags);

  const child = spawn(node, args);
  child.on('exit', common.mustCall(function(code, sig) {
    if (common.isWindows) {
      if (signals)
        assert.strictEqual(code, 0x80000003);
      else
        assert.strictEqual(code, 1);
    } else if (signals) {
      assert(signals.includes(sig), `Unexpected signal ${sig}`);
    } else {
      assert.strictEqual(sig, null);
    }
  }));
}
