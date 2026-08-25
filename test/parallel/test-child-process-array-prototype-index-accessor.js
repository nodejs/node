'use strict';
// Regression test for https://github.com/nodejs/node/issues/56531.
//
// Userland can install an index accessor on %Array.prototype% (for example
// `Object.defineProperty(Array.prototype, '2', { set() {} })`). Any subsequent
// `push()`/`unshift()` targeting that index assigns through the prototype
// chain, so the value is swallowed and a hole is left behind. child_process
// used to build its stdio descriptor list, its argument list and its
// environment that way, which lost arguments and environment entries and made
// the C++ layer read an `undefined` stdio descriptor.

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// The pollution has to happen in a separate process: it would otherwise break
// the test runner itself.
if (process.argv[2] === 'child') {
  const index = process.argv[3];
  Object.defineProperty(Array.prototype, index, {
    get() { return undefined; },
    set() {},
  });

  const cp = require('child_process');
  const printOk = ['-e', 'process.stdout.write("ok")'];
  const opts = { encoding: 'utf8' };

  if (cp.execFileSync(process.execPath, printOk, opts) !== 'ok') {
    process.exit(1);
  }

  if (cp.spawnSync(process.execPath, printOk, opts).stdout !== 'ok') {
    process.exit(2);
  }

  // Four entries so that a swallowed one is observable at every tested index.
  const env = { A: 'a', B: 'b', C: 'c', D: 'd' };
  const printEnv = [
    '-e', 'const { A, B, C, D } = process.env;' +
          'process.stdout.write(A + B + C + D);',
  ];
  if (cp.execFileSync(process.execPath, printEnv, { ...opts, env }) !== 'abcd') {
    process.exit(3);
  }

  // Goes through the shell, i.e. through `[shell, '-c', command]`.
  cp.exec('echo ok', opts, (err, stdout) => {
    process.exit(err || stdout.trim() !== 'ok' ? 4 : 0);
  });
  return;
}

// `[shell, '-c', command]` and the three default stdio descriptors mean the
// interesting indices are 0 to 3.
for (let index = 0; index <= 3; index++) {
  const result = spawnSync(process.execPath, [__filename, 'child', `${index}`], {
    encoding: 'utf8',
  });
  assert.strictEqual(
    result.status,
    0,
    `Array.prototype[${index}] accessor: exited with ${result.status}, ` +
    `signal ${result.signal}\n${result.stderr}`,
  );
}
