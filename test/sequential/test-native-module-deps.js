'use strict';

// This tests that all the native modules can be loaded independently
// Flags: --expose-internals

if (process.argv[3]) {
  require(process.argv[3]);
  return;
}

require('../common');
const {
  cachableBuiltins
} = require('internal/bootstrap/cache');
const { fork } = require('child_process');
const assert = require('assert');

for (const key of cachableBuiltins) {
  run(key);
}

function run(key) {
  const child = fork(__filename,
                     [ '--expose-internals', key ],
                     { silent: true }
  );

  let stdout = '';
  let stderr = '';
  child.stdout.on('data', (data) => (stdout += data.toString()));
  child.stderr.on('data', (data) => (stderr += data.toString()));
  child.on('close', (code) => {
    if (code === 0) {
      return;
    }
    console.log(`Failed to require ${key}`);
    console.log('----- stderr ----');
    console.log(stderr);
    console.log('----- stdout ----');
    console.log(stdout);
    assert.strictEqual(code, 0);
  });
}
