'use strict';
// Flags: --no-use-system-ca

// This tests that NODE_USE_SYSTEM_CA can be
// overridden by --no-use-system-ca.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { once } = require('events');
const { Worker } = require('worker_threads');
const fixtures = require('../common/fixtures');

const systemCerts = tls.getCACertificates('system');
if (systemCerts.length === 0) {
  common.skip('No trusted system certificates installed. Skip.');
}

async function runWorker({ execArgv, env }) {
  const worker = new Worker(
    fixtures.path('tls-get-ca-certificates-worker.js'),
    { execArgv, env },
  );
  worker.once('error', common.mustNotCall());
  const exitPromise = once(worker, 'exit');
  const messagePromise = once(worker, 'message');
  const [message] = await messagePromise;
  const [exitCode] = await exitPromise;
  assert.strictEqual(exitCode, 0);
  return message;
}

(async () => {
  // with --no-use-system-ca.
  assert.strictEqual(
    tls.getCACertificates('default').length,
    tls.getCACertificates('bundled').length,
  );

  const envEnabled = await runWorker({
    execArgv: [],
    env: { ...process.env, NODE_USE_SYSTEM_CA: '1' },
  });

  assert.strictEqual(envEnabled.systemLen, systemCerts.length);
  assert.strictEqual(
    envEnabled.defaultLen,
    envEnabled.bundledLen + envEnabled.systemLen,
  );

  const flagDisabled = await runWorker({
    execArgv: ['--no-use-system-ca'],
    env: { ...process.env, NODE_USE_SYSTEM_CA: '1' },
  });

  assert.strictEqual(flagDisabled.systemLen, systemCerts.length);
  assert.strictEqual(flagDisabled.defaultLen, flagDisabled.bundledLen);
})().then(common.mustCall());
