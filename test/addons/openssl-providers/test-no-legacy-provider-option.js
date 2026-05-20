'use strict';

const common = require('../../common');
const providers = require('./providers.cjs');
const assert = require('node:assert');
const { fork } = require('node:child_process');

const option = '--no-openssl-legacy-provider';
if (!process.execArgv.includes(option)) {
  const cp = fork(__filename, { execArgv: [ option ] });
  cp.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
  return;
}

providers.testProviderAbsent('legacy');
