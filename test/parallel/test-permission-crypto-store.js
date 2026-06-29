// Flags: --permission --allow-fs-read=* --allow-fs-write=* --allow-crypto-store --allow-child-process
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { hasOpenSSL3 } = require('../common/crypto');
if (!hasOpenSSL3)
  common.skip('requires OpenSSL 3.x');

// Verifies the crypto.store permission: allowed when --allow-crypto-store is
// set, can be dropped at runtime, and denied by default in a child process.

const assert = require('assert');
const dc = require('diagnostics_channel');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { createPrivateKey, generateKeyPairSync } = require('crypto');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const file = path.join(tmpdir.path, 'priv.pem');
fs.writeFileSync(
  file, generateKeyPairSync('ed25519').privateKey.export({
    format: 'pem', type: 'pkcs8',
  }));
const url = pathToFileURL(file);

assert.strictEqual(process.permission.has('crypto.store'), true);
assert.strictEqual(createPrivateKey(url).type, 'private');
assert.throws(() => createPrivateKey({
  href: file,
  protocol: 'pkcs11:',
}), { code: 'ERR_INVALID_URL' });

process.permission.drop('crypto.store');
assert.strictEqual(process.permission.has('crypto.store'), false);

const secret = 'store-permission-secret';
const messages = [];
dc.subscribe('node:permission-model:crypto-store', (message) => {
  messages.push(message);
});
assert.throws(
  () => createPrivateKey(new URL(`pkcs11:object=key;pin-value=${secret}`)),
  (error) => {
    assert.strictEqual(error.code, 'ERR_ACCESS_DENIED');
    assert.strictEqual(error.permission, 'CryptoStore');
    assert.strictEqual(error.resource, '');
    assert.doesNotMatch(error.stack, new RegExp(secret));
    return true;
  });
assert.strictEqual(messages.length, 1);
assert.strictEqual(messages[0].permission, 'CryptoStore');
assert.strictEqual(messages[0].resource, '');
assert.doesNotMatch(JSON.stringify(messages[0]), new RegExp(secret));

// Denied by default when the flag is not provided.
{
  const { status, stdout } = spawnSync(process.execPath, [
    '--permission', '--allow-fs-read=*',
    '-e', `try { require('crypto').createPrivateKey(new URL(${JSON.stringify(url.href)})); console.log('LOADED'); } catch (e) { console.log(e.code, e.permission); }`,
  ]);
  assert.strictEqual(status, 0);
  assert.match(stdout.toString(), /ERR_ACCESS_DENIED CryptoStore/);
}

// crypto.store grants the STORE loader authority to access files even when
// fs.read is not granted.
{
  const { status, stdout } = spawnSync(process.execPath, [
    '--permission', '--allow-crypto-store',
    '-e', `try { require('crypto').createPrivateKey(new URL(${JSON.stringify(url.href)})); console.log('LOADED'); } catch (e) { console.log(e.code, e.permission); }`,
  ]);
  assert.strictEqual(status, 0);
  assert.match(stdout.toString(), /LOADED/);
}

// OpenSSL tries the file loader before the loader identified by an opaque URI.
{
  const opaqueFile = 'pkcs11:priv.pem';
  fs.copyFileSync(file, path.join(tmpdir.path, opaqueFile));
  const { status, stdout } = spawnSync(process.execPath, [
    '--permission', '--allow-crypto-store',
    '-e', `try { require('crypto').createPrivateKey(new URL(${JSON.stringify(opaqueFile)})); console.log('LOADED'); } catch (e) { console.log(e.code, e.permission); }`,
  ], { cwd: tmpdir.path });
  assert.strictEqual(status, 0);
  assert.match(stdout.toString(), /LOADED/);
}
