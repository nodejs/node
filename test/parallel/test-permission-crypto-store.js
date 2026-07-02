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

process.permission.drop('crypto.store');
assert.strictEqual(process.permission.has('crypto.store'), false);
assert.throws(() => createPrivateKey(url), { code: 'ERR_ACCESS_DENIED' });

// Denied by default when the flag is not provided.
{
  const { status, stdout } = spawnSync(process.execPath, [
    '--permission', '--allow-fs-read=*',
    '-e', `try { require('crypto').createPrivateKey(new URL(${JSON.stringify(url.href)})); console.log('LOADED'); } catch (e) { console.log(e.code, e.permission); }`,
  ]);
  assert.strictEqual(status, 0);
  assert.match(stdout.toString(), /ERR_ACCESS_DENIED CryptoStore/);
}

// file: store URIs also require fs.read permission.
{
  const { status, stdout } = spawnSync(process.execPath, [
    '--permission', '--allow-crypto-store',
    '-e', `try { require('crypto').createPrivateKey(new URL(${JSON.stringify(url.href)})); console.log('LOADED'); } catch (e) { console.log(e.code, e.permission); }`,
  ]);
  assert.strictEqual(status, 0);
  assert.match(stdout.toString(), /ERR_ACCESS_DENIED FileSystemRead/);
}
