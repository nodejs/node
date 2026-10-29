'use strict';

// Flags: --expose-internals

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
const assert = require('node:assert');
const crypto = require('node:crypto');
const fixtures = require('../common/fixtures');
const {
  prepareAsymmetricKey,
  getKeyObjectHandle,
  kConsumePublic,
  kConsumePrivate,
  kCreatePublic,
  kCreatePrivate,
} = require('internal/crypto/keys');

const pem = fixtures.readKey('ec_p256_private.pem');
const keyObject = crypto.createPrivateKey(pem);
const contexts = [kConsumePublic, kConsumePrivate, kCreatePublic, kCreatePrivate];

for (const input of [
  pem,
  new Uint8Array(pem),
  new DataView(Uint8Array.from(pem).buffer),
  Uint8Array.from(pem).buffer,
  new SharedArrayBuffer(pem.length),
]) {
  Object.defineProperty(input, 'key', { get: common.mustNotCall() });
  Object.defineProperty(input, 'format', { get: common.mustNotCall() });
  Object.defineProperty(input, Symbol.toStringTag, { value: 'KeyObject' });
  for (const context of contexts) {
    assert.strictEqual(prepareAsymmetricKey(input, context).data, input);
    assert.strictEqual(prepareAsymmetricKey({ key: input }, context).data, input);
  }
}

for (const context of [kConsumePublic, kConsumePrivate, kCreatePublic]) {
  const options = { key: keyObject, format: 'raw-public', asymmetricKeyType: 'invalid' };
  assert.strictEqual(prepareAsymmetricKey(options, context).data, getKeyObjectHandle(keyObject));
}

{
  const reads = [];
  const values = { key: pem.toString(), encoding: 'utf8', format: 'pem' };
  const options = {};
  for (const name of ['key', 'encoding', 'format', 'properties', 'type', 'cipher', 'passphrase']) {
    Object.defineProperty(options, name, {
      get() {
        reads.push(name);
        return values[name];
      },
    });
  }
  assert.deepStrictEqual(prepareAsymmetricKey(options, kConsumePrivate).data, pem);
  assert.deepStrictEqual(reads, [
    'key', 'encoding', 'format', 'properties', 'format', 'type', 'cipher', 'passphrase', 'encoding',
  ]);
}

{
  const input = {
    key: pem.toString(),
    encoding: 'invalid',
    get format() { return 'pem'; },
    get type() { return assert.fail('String conversion must precede encoding option parsing'); },
  };
  assert.throws(() => prepareAsymmetricKey(input, kConsumePrivate), { code: 'ERR_UNKNOWN_ENCODING' });
}

{
  const raw = keyObject.export({ format: 'raw-private' });
  const input = { key: raw, format: 'raw-private', asymmetricKeyType: 'ec', namedCurve: 'P-256' };
  assert.strictEqual(prepareAsymmetricKey(input, kConsumePrivate).data, raw);
  assert.throws(() => prepareAsymmetricKey({ ...input, key: raw.toString('hex') }, kConsumePrivate), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

for (const encoding of [undefined, 'buffer', 'utf8', 'hex', 'base64']) {
  const inputEncoding = encoding === 'buffer' ? 'utf8' : encoding;
  const passphrase = Buffer.from('password');
  const result = prepareAsymmetricKey({
    key: pem.toString(inputEncoding),
    passphrase: passphrase.toString(inputEncoding),
    encoding,
  }, kConsumePrivate);
  assert.deepStrictEqual(result.data, pem);
  assert.deepStrictEqual(result.passphrase, passphrase);
}

for (const [options, optionName] of [
  [{ format: 'invalid' }, 'format'],
  [{ format: 'der', type: 'invalid' }, 'type'],
]) {
  assert.throws(() => prepareAsymmetricKey({ key: pem, ...options }, kConsumePrivate, 'options.key'), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: new RegExp(`options\\.key\\.${optionName}`),
  });
}
