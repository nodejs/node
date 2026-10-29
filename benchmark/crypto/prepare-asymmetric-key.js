'use strict';

const common = require('../common.js');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { hasOpenSSL, isBoringSSL } = require('../../test/common/crypto.js');

const inputs = [
  'public-keyobject', 'public-keyobject-wrapped', 'public-pem-buffer-wrapped',
  'private-keyobject', 'private-keyobject-wrapped',
  'private-pem-string', 'private-pem-buffer', 'private-pem-arraybuffer',
  'private-pem-string-wrapped', 'private-pem-buffer-wrapped', 'private-pem-arraybuffer-wrapped',
  'private-jwk',
  'public-der-pkcs1-bytes', 'private-der-pkcs1-bytes', 'private-pem-pkcs8-bytes',
  'private-der-pkcs8-bytes', 'private-der-pkcs8-base64', 'private-der-pkcs8-hex',
  'public-der-spki-bytes', 'private-der-sec1-bytes',
  'raw-public-ec-buffer', 'raw-public-ed25519-buffer',
  'raw-private-ec-buffer', 'raw-private-ec-arraybuffer', 'raw-private-ed25519-buffer',
  'pem-passphrase-absent', 'pem-passphrase-empty', 'pem-passphrase-string',
  'pem-passphrase-base64', 'pem-passphrase-buffer', 'pem-passphrase-arraybuffer',
  'file-passphrase-absent', 'file-passphrase-string', 'file-direct',
  'provider-passphrase-absent', 'provider-passphrase-absent-properties',
  'provider-passphrase-empty', 'provider-passphrase-string', 'provider-passphrase-base64',
  'provider-passphrase-buffer', 'provider-passphrase-arraybuffer', 'provider-direct',
];

if (hasOpenSSL(3, 5) || isBoringSSL)
  inputs.push('raw-seed-ml-dsa-44-buffer');

const bench = common.createBenchmark(main, {
  input: inputs,
  context: ['consume-public', 'consume-private', 'create-public'],
  n: [1e6],
}, {
  flags: ['--expose-internals'],
  combinationFilter({ input, context }) {
    if (input === 'private-keyobject' || input === 'private-keyobject-wrapped')
      return true;
    if (input.startsWith('public-') || input.startsWith('raw-public-'))
      return context === 'consume-public';
    return context === 'consume-private';
  },
});

function main({ input, context, n }) {
  const {
    prepareAsymmetricKey,
    kConsumePublic,
    kConsumePrivate,
    kCreatePublic,
  } = require('internal/crypto/keys');
  const ctx = {
    'consume-public': kConsumePublic,
    'consume-private': kConsumePrivate,
    'create-public': kCreatePublic,
  }[context];
  const fixtureDir = path.resolve(__dirname, '../../test/fixtures/keys');
  const privateKeyPath = path.join(fixtureDir, 'rsa_private_2048.pem');
  const privateKey = crypto.createPrivateKey(fs.readFileSync(privateKeyPath));
  const publicKey = crypto.createPublicKey(privateKey);
  const pem = privateKey.export({ format: 'pem', type: 'pkcs8' });
  const buffer = Buffer.from(pem);
  const arraybuffer = Uint8Array.from(buffer).buffer;
  const fileURL = pathToFileURL(privateKeyPath);
  const providerURL = new URL('pkcs11:object=signing-key;type=private');
  const passphrase = Buffer.from('password');

  let key;
  switch (input) {
    case 'public-keyobject': key = publicKey; break;
    case 'public-keyobject-wrapped': key = { key: publicKey }; break;
    case 'public-pem-buffer-wrapped':
      key = { key: Buffer.from(publicKey.export({ format: 'pem', type: 'spki' })) };
      break;
    case 'private-keyobject': key = privateKey; break;
    case 'private-keyobject-wrapped': key = { key: privateKey }; break;
    case 'private-pem-string': key = pem; break;
    case 'private-pem-buffer': key = buffer; break;
    case 'private-pem-arraybuffer': key = arraybuffer; break;
    case 'private-pem-string-wrapped': key = { key: pem }; break;
    case 'private-pem-buffer-wrapped': key = { key: buffer }; break;
    case 'private-pem-arraybuffer-wrapped': key = { key: arraybuffer }; break;
    case 'private-jwk': key = { key: privateKey.export({ format: 'jwk' }), format: 'jwk' }; break;
    case 'public-der-pkcs1-bytes': key = { key: publicKey, format: 'der', type: 'pkcs1' }; break;
    case 'private-der-pkcs1-bytes': key = { key: privateKey, format: 'der', type: 'pkcs1' }; break;
    case 'private-pem-pkcs8-bytes': key = { key: privateKey, format: 'pem', type: 'pkcs8' }; break;
    case 'private-der-pkcs8-bytes': key = { key: privateKey, format: 'der', type: 'pkcs8' }; break;
    case 'private-der-pkcs8-base64':
      key = { key: privateKey, format: 'der', type: 'pkcs8', encoding: 'base64' };
      break;
    case 'private-der-pkcs8-hex':
      key = { key: privateKey, format: 'der', type: 'pkcs8', encoding: 'hex' };
      break;
    case 'public-der-spki-bytes': key = { key: publicKey, format: 'der', type: 'spki' }; break;
    case 'private-der-sec1-bytes':
      key = {
        key: crypto.createPrivateKey(fs.readFileSync(path.join(fixtureDir, 'ec_p256_private.pem'))),
        format: 'der', type: 'sec1',
      };
      break;
    case 'raw-public-ec-buffer':
    case 'raw-public-ed25519-buffer':
    case 'raw-private-ec-buffer':
    case 'raw-private-ec-arraybuffer':
    case 'raw-private-ed25519-buffer': {
      const asymmetricKeyType = input.includes('-ec-') ? 'ec' : 'ed25519';
      const fixture = asymmetricKeyType === 'ec' ? 'ec_p256_private.pem' : 'ed25519_private.pem';
      let rawKey = crypto.createPrivateKey(fs.readFileSync(path.join(fixtureDir, fixture)));
      const format = input.startsWith('raw-public-') ? 'raw-public' : 'raw-private';
      if (format === 'raw-public') rawKey = crypto.createPublicKey(rawKey);
      const bytes = rawKey.export({ format });
      key = {
        key: input.endsWith('-arraybuffer') ? Uint8Array.from(bytes).buffer : bytes,
        format,
        asymmetricKeyType,
        namedCurve: asymmetricKeyType === 'ec' ? 'prime256v1' : undefined,
      };
      break;
    }
    case 'raw-seed-ml-dsa-44-buffer': {
      const seedKey = crypto.createPrivateKey(
        fs.readFileSync(path.join(fixtureDir, 'ml_dsa_44_private_seed_only.pem')));
      key = { key: seedKey.export({ format: 'raw-seed' }), format: 'raw-seed', asymmetricKeyType: 'ml-dsa-44' };
      break;
    }
    case 'pem-passphrase-absent': key = { key: privateKey }; break;
    case 'pem-passphrase-empty': key = { key: privateKey, passphrase: '' }; break;
    case 'pem-passphrase-string': key = { key: privateKey, passphrase: 'password' }; break;
    case 'pem-passphrase-base64':
      key = { key: privateKey, passphrase: passphrase.toString('base64'), encoding: 'base64' };
      break;
    case 'pem-passphrase-buffer': key = { key: privateKey, passphrase }; break;
    case 'pem-passphrase-arraybuffer':
      key = { key: privateKey, passphrase: Uint8Array.from(passphrase).buffer };
      break;
    case 'file-passphrase-absent': key = { key: fileURL }; break;
    case 'file-passphrase-string': key = { key: fileURL, passphrase: 'password' }; break;
    case 'file-direct': key = fileURL; break;
    case 'provider-passphrase-absent': key = { key: providerURL }; break;
    case 'provider-passphrase-absent-properties': key = { key: providerURL, properties: 'provider=default' }; break;
    case 'provider-passphrase-empty': key = { key: providerURL, passphrase: '' }; break;
    case 'provider-passphrase-string': key = { key: providerURL, passphrase: 'password' }; break;
    case 'provider-passphrase-base64':
      key = { key: providerURL, passphrase: passphrase.toString('base64'), encoding: 'base64' };
      break;
    case 'provider-passphrase-buffer': key = { key: providerURL, passphrase }; break;
    case 'provider-passphrase-arraybuffer':
      key = { key: providerURL, passphrase: Uint8Array.from(passphrase).buffer };
      break;
    case 'provider-direct': key = providerURL; break;
    default: throw new Error(`Unsupported input: ${input}`);
  }

  if (input.startsWith('pem-passphrase-')) {
    const bytes = key.passphrase === undefined ? undefined :
      typeof key.passphrase === 'string' ? Buffer.from(key.passphrase, key.encoding) :
        Buffer.from(key.passphrase);
    key = {
      key: Buffer.from(privateKey.export({
        format: 'pem', type: 'pkcs8',
        ...(bytes === undefined ? {} : { cipher: 'aes-256-cbc', passphrase: bytes }),
      })),
      format: 'pem', type: 'pkcs8', passphrase: key.passphrase, encoding: key.encoding,
    };
  } else if (key.key instanceof crypto.KeyObject && key.format !== undefined) {
    const bytes = Buffer.from(key.key.export({ format: key.format, type: key.type }));
    key.key = key.encoding === undefined ? bytes : bytes.toString(key.encoding);
  }

  prepareAsymmetricKey(key, ctx);
  bench.start();
  for (let index = 0; index < n; index++) {
    prepareAsymmetricKey(key, ctx);
  }
  bench.end(n);
}
