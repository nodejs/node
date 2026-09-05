// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const https = require('https');
const tls = require('tls');
const { internalBinding } = require('internal/test/binding');

process.on('warning', (warning) => {
  if (warning.code === 'DEP0183')
    throw warning;
});

// DEP0183: OpenSSL engine-based APIs have reached End-of-Life.
assert.strictEqual(Object.hasOwn(crypto, 'setEngine'), false);
import('node:crypto').then(common.mustCall((esmCrypto) => {
  assert.strictEqual(Object.hasOwn(esmCrypto, 'setEngine'), false);
}));

for (const name of [
  'ENGINE_METHOD_RSA',
  'ENGINE_METHOD_DSA',
  'ENGINE_METHOD_DH',
  'ENGINE_METHOD_RAND',
  'ENGINE_METHOD_CIPHERS',
  'ENGINE_METHOD_DIGESTS',
  'ENGINE_METHOD_PKEY_METHS',
  'ENGINE_METHOD_PKEY_ASN1_METHS',
  'ENGINE_METHOD_EC',
  'ENGINE_METHOD_ALL',
  'ENGINE_METHOD_NONE',
]) {
  assert.strictEqual(Object.hasOwn(crypto.constants, name), false);
}

const binding = internalBinding('crypto');
assert.strictEqual(Object.hasOwn(binding, 'setEngine'), false);
const secureContext = new binding.SecureContext();
assert.strictEqual('setEngineKey' in secureContext, false);
assert.strictEqual('setClientCertEngine' in secureContext, false);

const engineError = {
  code: 'ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED',
  message: 'Custom engines not supported by this version of Node.js',
};
const engineOptions = [
  { clientCertEngine: 'engine' },
  { clientCertEngine: 0 },
  { privateKeyEngine: 'engine' },
  { privateKeyEngine: false },
  { privateKeyIdentifier: 'key' },
  { privateKeyIdentifier: '' },
  { privateKeyIdentifier: 'key', privateKeyEngine: 'engine' },
];
const existingContext = tls.createSecureContext();

// The removed TLS options remain recognized so they cannot appear to work.
for (const options of engineOptions) {
  assert.throws(() => tls.createSecureContext(options), engineError);
  assert.throws(() => tls.createServer(options), engineError);
  assert.throws(
    () => tls.connect({ port: 443, secureContext: existingContext, ...options }),
    engineError,
  );
  assert.throws(
    () => new tls.TLSSocket(undefined, { secureContext: existingContext, ...options }),
    engineError,
  );
  assert.throws(
    () => https.request({ host: 'localhost', port: 443, agent: false, ...options }),
    engineError,
  );
}

// HTTPS rejects the options before a pooled socket could hide their use.
const agent = new https.Agent();
const options = { host: 'example.com', port: 443 };
for (const removedOption of [
  'clientCertEngine',
  'privateKeyEngine',
  'privateKeyIdentifier',
]) {
  assert.throws(
    () => new https.Agent({ [removedOption]: 'engine' }),
    engineError,
  );
  assert.throws(
    () => agent.getName({ ...options, [removedOption]: 'engine' }),
    engineError,
  );
}
