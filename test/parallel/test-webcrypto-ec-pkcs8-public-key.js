'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createPrivateKey, KeyObject } = require('crypto');
const fixtures = require('../common/fixtures');
const { subtle } = globalThis.crypto;

function der(tag, ...parts) {
  const body = Buffer.concat(parts);
  const length = body.length < 128 ? [body.length] : [0x81, body.length];
  return Buffer.concat([Buffer.from([tag, ...length]), body]);
}

(async () => {
  for (const [curve, oid] of [
    ['p256', '06082a8648ce3d030107'],
    ['p384', '06052b81040022'],
    ['p521', '06052b81040023'],
  ]) {
    const privateKey = createPrivateKey(fixtures.readKey(`ec_${curve}_private.pem`));
    const expected = privateKey.export({ type: 'pkcs8', format: 'der' });
    const jwk = privateKey.export({ format: 'jwk' });
    const curveOid = Buffer.from(oid, 'hex');
    const algorithmIdentifier = der(
      0x30, Buffer.from('06072a8648ce3d0201', 'hex'), curveOid);

    for (const includeParameters of [false, true]) {
      const ecPrivateKey = der(
        0x30, Buffer.from('020101', 'hex'), der(0x04, Buffer.from(jwk.d, 'base64url')),
        includeParameters ? der(0xa0, curveOid) : Buffer.alloc(0));
      const privateOnly = der(
        0x30, Buffer.from('020100', 'hex'), algorithmIdentifier, der(0x04, ecPrivateKey));
      for (const name of ['ECDSA', 'ECDH']) {
        const key = await subtle.importKey(
          'pkcs8', privateOnly, { name, namedCurve: jwk.crv }, true,
          name === 'ECDSA' ? ['sign'] : ['deriveBits']);
        const original = KeyObject.from(key).export({ type: 'pkcs8', format: 'der' });
        const actual = Buffer.from(await subtle.exportKey('pkcs8', key));
        assert.deepStrictEqual(actual, expected);
        assert.deepStrictEqual(
          KeyObject.from(key).export({ type: 'pkcs8', format: 'der' }), original);
      }
    }
  }
})().then(common.mustCall());
