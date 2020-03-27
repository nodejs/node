'use strict';

// Notarization on macOS requires all binaries to be signed.
// We sign our own binaries but check here if any binaries from our dependencies
// (e.g. npm) are signed.
const common = require('../common');

if (!common.isOSX) {
  common.skip('macOS specific test');
}

const assert = require('assert');
const { spawnSync } = require('child_process');
const path = require('path');

const debuglog = require('util').debuglog('test');

const binaries = [
  'deps/npm/node_modules/term-size/vendor/macos/term-size',
];

for (const testbin of binaries) {
  const bin = path.resolve(__dirname, '..', '..', testbin);
  debuglog(`Checking ${bin}`);
  const cp = spawnSync('codesign', [ '-vvvv', bin ], { encoding: 'utf8' });
  debuglog(cp.stdout);
  debuglog(cp.stderr);
  assert.strictEqual(cp.signal, null);
  assert.strictEqual(cp.status, 0, `${bin} does not appear to be signed.\n` +
                     `${cp.stdout}\n${cp.stderr}`);
}
