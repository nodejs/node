'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');
if (!hasOpenSSL(3))
  common.skip('this test requires OpenSSL >= 3.x');

// OpenSSL 3 builds its table of algorithm names on first use. If several
// threads use OpenSSL for the first time at once, an RSA-PSS key decoded while
// the table is still being built can miss the RSA-PSS name, and every later
// RSA-PSS decode in the process fails as well. Here Workers decode an RSA-PSS
// certificate while the root certificate loader or a thread pool crypto job
// uses OpenSSL for the first time.

const assert = require('assert');
const { X509Certificate } = require('crypto');
const { once } = require('events');
const { Worker, parentPort, workerData } = require('worker_threads');
const fixtures = require('../common/fixtures');

if (workerData) {
  parentPort.postMessage('ready');
  Atomics.wait(workerData.barrier, 0, 0);
  const cert = new X509Certificate(workerData.pem);
  assert.strictEqual(cert.publicKey.asymmetricKeyType, 'rsa-pss');
  assert(Buffer.isBuffer(cert.toLegacyObject().pubkey));
} else if (process.argv[2] === 'child') {
  // OpenSSL must not have been used in this process before the barrier opens.
  const pem = fixtures.readKey('rsa_pss_cert_2048.pem');
  const barrier = new Int32Array(new SharedArrayBuffer(4));
  const workers = Array.from({ length: 8 }, () =>
    new Worker(__filename, { workerData: { pem, barrier } }));
  Promise.all(workers.map((w) => once(w, 'message'))).then(common.mustCall(() => {
    if (process.argv[3] === 'tls') {
      require('tls');
    } else {
      require('crypto').pbkdf2('password', '0123456789abcdef', 1000, 32,
                               'sha256', common.mustSucceed());
    }
    Atomics.store(barrier, 0, 1);
    Atomics.notify(barrier, 0);
  }));
} else {
  (async () => {
    for (const mode of ['tls', 'pbkdf2']) {
      for (let round = 0; round < 3; round++) {
        const children = Array.from({ length: 4 }, () =>
          common.spawnPromisified(process.execPath,
                                  [__filename, 'child', mode]));
        for (const { code, signal, stderr } of await Promise.all(children)) {
          assert.strictEqual(signal, null, `${mode}: ${stderr}`);
          assert.strictEqual(code, 0, `${mode}: ${stderr}`);
        }
      }
    }
  })().then(common.mustCall());
}
