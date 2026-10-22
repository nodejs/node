'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// OpenSSL 3 builds its table of algorithm names on first use. If that first
// use happens on a thread started by Node.js (a thread pool crypto job, the
// root certificate loader or a Worker) while the main thread decodes an
// RSA-PSS key, the decode can miss the RSA-PSS name, and every later RSA-PSS
// decode in the process fails as well.

const assert = require('assert');
const fixtures = require('../common/fixtures');

if (process.argv[2] === 'child') {
  const { X509Certificate, pbkdf2 } = require('crypto');
  const pem = fixtures.readKey('rsa_pss_cert_2048.pem');
  // OpenSSL must not have been used on this thread before this point.
  if (process.argv[3] === 'tls') {
    require('tls');
  } else if (process.argv[3] === 'worker') {
    const { Worker } = require('worker_threads');
    const started = new Int32Array(new SharedArrayBuffer(4));
    new Worker(`
      const { workerData } = require('worker_threads');
      const { createHash } = require('crypto');
      Atomics.store(workerData, 0, 1);
      Atomics.notify(workerData, 0);
      createHash('sha256');
    `, { eval: true, workerData: started });
    Atomics.wait(started, 0, 0);
  } else {
    pbkdf2('password', '0123456789abcdef', 1000, 32, 'sha256',
           common.mustSucceed());
  }
  const cert = new X509Certificate(pem);
  assert.strictEqual(cert.publicKey.asymmetricKeyType, 'rsa-pss');
  return;
}

const { hasOpenSSL } = require('../common/crypto');
if (!hasOpenSSL(3))
  common.skip('this test requires OpenSSL 3.x');

(async () => {
  for (const mode of ['pbkdf2', 'tls', 'worker']) {
    for (let round = 0; round < 5; round++) {
      const children = Array.from({ length: 4 }, () =>
        common.spawnPromisified(process.execPath, [__filename, 'child', mode]));
      for (const { code, signal, stderr } of await Promise.all(children)) {
        assert.strictEqual(signal, null, `${mode}: ${stderr}`);
        assert.strictEqual(code, 0, `${mode}: ${stderr}`);
      }
    }
  }
})().then(common.mustCall());
