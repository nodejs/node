// Flags: --experimental-dtls --no-warnings

// Test: passphrase support for encrypted private keys.
//
// An encrypted key could not be used at all before: there was nowhere to put
// the passphrase, and the failure reported PEM_read_bio_PrivateKey rather than
// anything about decryption. The option matches node:tls -- same name, string
// only, ignored when the key is not encrypted.

import { hasCrypto, mustNotCall, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const PASSPHRASE = 'correct horse battery staple';

// Generate an encrypted key and a matching self-signed certificate rather
// than checking a fixture in, so the passphrase is visible in the test.
const dir = mkdtempSync(join(tmpdir(), 'dtls-passphrase-'));
let encryptedKey;
let encryptedCert;
try {
  const keyPath = join(dir, 'key.pem');
  const certPath = join(dir, 'cert.pem');
  execFileSync('openssl', [
    'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048',
    '-aes-256-cbc', '-pass', `pass:${PASSPHRASE}`, '-out', keyPath,
  ], { stdio: 'ignore' });
  execFileSync('openssl', [
    'req', '-new', '-x509', '-key', keyPath, '-passin', `pass:${PASSPHRASE}`,
    '-subj', '/CN=dtls-passphrase-test', '-days', '1', '-out', certPath,
  ], { stdio: 'ignore' });
  encryptedKey = readFileSync(keyPath, 'utf8');
  encryptedCert = readFileSync(certPath, 'utf8');
} catch {
  rmSync(dir, { recursive: true, force: true });
  skip('openssl command not available');
}
rmSync(dir, { recursive: true, force: true });

assert.match(encryptedKey, /^-----BEGIN ENCRYPTED PRIVATE KEY-----/);

// The correct passphrase decrypts the key, and the endpoint serves with it.
{
  const echoed = Promise.withResolvers();

  const endpoint = listen((session) => {
    session.onmessage = (data) => session.send(data);
  }, {
    cert: encryptedCert,
    key: encryptedKey,
    passphrase: PASSPHRASE,
    host: '127.0.0.1',
    port: 0,
  });

  // Self-signed and not in any CA store, so verification is off here; the
  // point of the test is that the encrypted key loaded and can sign.
  const client = connect('127.0.0.1', endpoint.address.port, {
    rejectUnauthorized: false,
  });

  await client.opened;
  client.onmessage = (data) => echoed.resolve(data.toString());
  client.send('hello');

  assert.strictEqual(await echoed.promise, 'hello');

  await client.close();
  await endpoint.close();
}

// The wrong passphrase and no passphrase both fail, and say why. The code
// comes from OpenSSL and could shift if the bundled version changes.
for (const passphrase of [`${PASSPHRASE}-wrong`, undefined]) {
  assert.throws(() => listen(mustNotCall(), {
    cert: encryptedCert,
    key: encryptedKey,
    passphrase,
    host: '127.0.0.1',
    port: 0,
  }), { code: 'ERR_OSSL_BAD_DECRYPT' });
}

// A passphrase supplied for a key that is not encrypted is ignored.
{
  const cert = fixtures.readKey('agent1-cert.pem').toString();
  const key = fixtures.readKey('agent1-key.pem').toString();

  for (const passphrase of ['not needed', undefined, null]) {
    const endpoint = listen(mustNotCall(), {
      cert, key, passphrase, host: '127.0.0.1', port: 0,
    });
    await endpoint.close();
  }
}

// The passphrase is a string, matching node:tls, even though key and cert
// also accept a Buffer.
for (const passphrase of [123, Buffer.from(PASSPHRASE), {}, []]) {
  assert.throws(() => listen(mustNotCall(), {
    cert: encryptedCert,
    key: encryptedKey,
    passphrase,
    host: '127.0.0.1',
    port: 0,
  }), { code: 'ERR_INVALID_ARG_TYPE' });
}

// connect() takes it too, for client certificates.
{
  const endpoint = listen(mustNotCall(), {
    cert: fixtures.readKey('agent1-cert.pem').toString(),
    key: fixtures.readKey('agent1-key.pem').toString(),
    host: '127.0.0.1',
    port: 0,
  });

  assert.throws(() => connect('127.0.0.1', endpoint.address.port, {
    cert: encryptedCert,
    key: encryptedKey,
    passphrase: `${PASSPHRASE}-wrong`,
    rejectUnauthorized: false,
  }), { code: 'ERR_OSSL_BAD_DECRYPT' });

  await endpoint.close();
}
