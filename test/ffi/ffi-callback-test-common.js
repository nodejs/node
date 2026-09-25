'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

function spawnAbortingChild(source) {
  const args = ['-e', source];
  if (common.isWindows) {
    return spawnSync(process.execPath, args, { encoding: 'utf8' });
  }

  // Avoid writing core files for these intentional aborts.
  return spawnSync('/bin/sh', [
    '-c', 'ulimit -c 0 && exec "$@"',
    'sh', process.execPath, ...args,
  ], { encoding: 'utf8' });
}

function assertAborts(source, message) {
  const { stderr, status, signal } = spawnAbortingChild(source);
  assert.ok(common.nodeProcessAborted(status, signal),
            `status: ${status}, signal: ${signal}
stderr: ${stderr}`);
  assert.match(stderr, message);
}

function assertCallbackAborts(callbackBody, message) {
  assertAborts(
    `'use strict';
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require(${JSON.stringify(require.resolve('./ffi-test-common'))});
const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
const callback = lib.registerCallback(
  { arguments: ['i32'], return: 'i32' },
  () => { ${callbackBody} },
);
functions.call_int_callback(callback, 21);`,
    message,
  );
}

module.exports = {
  assertAborts,
  assertCallbackAborts,
};
