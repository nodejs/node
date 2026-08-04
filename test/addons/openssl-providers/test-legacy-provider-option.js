'use strict';

const common = require('../../common');
const providers = require('./providers.cjs');
const assert = require('node:assert');
const { fork } = require('node:child_process');
const { getFips } = require('node:crypto');

const option = '--openssl-legacy-provider';
const execArgv = process.execArgv;
if (!execArgv.includes(option)) {
  const cp = fork(__filename, { execArgv: [ option ] });
  cp.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
  return;
}

// Enabling FIPS will make all legacy provider algorithms unavailable.
if (getFips()) {
  common.skip('this test cannot be run in FIPS mode');
}
providers.testProviderPresent('legacy');
