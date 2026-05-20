'use strict';
// Flags: --no-use-system-ca

// This tests that --use-system-ca is an Environment option (i.e. can be
// enabled/disabled in workers) and that it affects tls.getCACertificates('default').

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
async function runWorker(execArgv) {
  const worker = new Worker(
    fixtures.path('tls-get-ca-certificates-worker.js'),
    { execArgv },
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
  const withSystemCA = await runWorker(['--use-system-ca']);
  assert.strictEqual(withSystemCA.systemLen, systemCerts.length);

  assert.strictEqual(
    withSystemCA.defaultLen,
    withSystemCA.bundledLen + withSystemCA.systemLen,
  );

  assert.strictEqual(
    tls.getCACertificates('default').length,
    tls.getCACertificates('bundled').length,
  );
})().then(common.mustCall());
