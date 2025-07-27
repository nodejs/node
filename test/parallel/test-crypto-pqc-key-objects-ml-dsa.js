'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

const assert = require('assert');
const {
  createPublicKey,
  createPrivateKey,
} = require('crypto');

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

for (const [asymmetricKeyType, pubLen] of [
  ['ml-dsa-44', 1312], ['ml-dsa-65', 1952], ['ml-dsa-87', 2592],
]) {
  const keys = {
    public: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'public'), 'ascii'),
    private: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private'), 'ascii'),
    private_seed_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_seed_only'), 'ascii'),
    private_priv_only: fixtures.readKey(getKeyFileName(asymmetricKeyType, 'private_priv_only'), 'ascii'),
  };

  function assertJwk(jwk) {
    assert.strictEqual(jwk.kty, 'AKP');
    assert.strictEqual(jwk.alg, asymmetricKeyType.toUpperCase());
    assert.ok(jwk.pub);
    assert.strictEqual(Buffer.from(jwk.pub, 'base64url').byteLength, pubLen);
  }

  function assertPublicJwk(jwk) {
    assertJwk(jwk);
    assert.ok(!jwk.priv);
  }

  function assertPrivateJwk(jwk) {
    assertJwk(jwk);
    assert.ok(jwk.priv);
    assert.strictEqual(Buffer.from(jwk.priv, 'base64url').byteLength, 32);
  }

  function assertKey(key) {
    assert.deepStrictEqual(key.asymmetricKeyDetails, {});
    assert.strictEqual(key.asymmetricKeyType, asymmetricKeyType);
    assert.strictEqual(key.equals(key), true);
    assert.deepStrictEqual(key, key);
  }

  function assertPublicKey(key) {
    assertKey(key);
    assert.strictEqual(key.type, 'public');
    assert.strictEqual(key.export({ format: 'pem', type: 'spki' }), keys.public);
    key.export({ format: 'der', type: 'spki' });
    const jwk = key.export({ format: 'jwk' });
    assertPublicJwk(jwk);
    assert.strictEqual(key.equals(createPublicKey({ format: 'jwk', key: jwk })), true);
  }

  function assertPrivateKey(key, hasSeed) {
    assertKey(key);
    assert.strictEqual(key.type, 'private');
    assertPublicKey(createPublicKey(key));
    key.export({ format: 'der', type: 'pkcs8' });
    if (hasSeed) {
      assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private);
    } else {
      assert.strictEqual(key.export({ format: 'pem', type: 'pkcs8' }), keys.private_priv_only);
    }
    if (hasSeed) {
      const jwk = key.export({ format: 'jwk' });
      assertPrivateJwk(jwk);
      assert.strictEqual(key.equals(createPrivateKey({ format: 'jwk', key: jwk })), true);
      assert.ok(createPublicKey({ format: 'jwk', key: jwk }));
    } else {
      assert.throws(() => key.export({ format: 'jwk' }),
                    { code: 'ERR_CRYPTO_OPERATION_FAILED', message: 'key does not have an available seed' });
    }
  }

  if (!hasOpenSSL(3, 5)) {
    assert.throws(() => createPublicKey(keys.public), {
      code: hasOpenSSL(3) ? 'ERR_OSSL_EVP_DECODE_ERROR' : 'ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM',
    });

    for (const pem of [keys.private, keys.private_seed_only, keys.private_priv_only]) {
      assert.throws(() => createPrivateKey(pem), {
        code: hasOpenSSL(3) ? 'ERR_OSSL_UNSUPPORTED' : 'ERR_OSSL_EVP_UNSUPPORTED_ALGORITHM',
      });
    }
  } else {
    const publicKey = createPublicKey(keys.public);
    assertPublicKey(publicKey);

    {
      for (const [pem, hasSeed] of [
        [keys.private, true],
        [keys.private_seed_only, true],
        [keys.private_priv_only, false],
      ]) {
        const pubFromPriv = createPublicKey(pem);
        assertPublicKey(pubFromPriv);
        assertPrivateKey(createPrivateKey(pem), hasSeed);
        assert.strictEqual(pubFromPriv.equals(publicKey), true);
      }
    }
  }
}

{
  const format = 'jwk';
  const jwk = {
    priv: '9_uqvxH0WKJFgfLyse1a1des2bwPgsHctl_jCt5AfEo',
    kty: 'AKP',
    alg: 'ML-DSA-44',
    pub: 'SXghXj9P-DJ5eznNa_zLJxRxpa0mt86WlIid0EVEv1qraLBkC1UKevSZrjtgo1QUEN0oa3tP-HyYj8Onnc1zEnxsSoeC5A-PgywKgYuZP58' +
         '1wGPS-cbA2-5acsg-YUi_9fDkLR5YOTQQ3Iu952K1m8w0QDIBxZjecm32HgkD56CCC6ZyBOwfx9qcNUeO0aImya1igzL2_LRsqomogl9Oud' +
         'uWhtussAavGlAK7ZR4_4lmyjWcdeIc-z--iy42biV5d_tnopfNTJFlycBKinZu3h0lr4-ldl6apGDIyvSdZulhgj_j6jgEX-AgQZgS93ctx' +
         '680GvROkBL7YI_3iXW3REWzVgS9HLasagEi2h6-RYQ9RzgUTODbei5fNRj3bNSqr8IKTZ08DCsRasN61TGwE3F7meoauw2NYkV51mhTxIaf' +
         'whLWJrRA4C09Y-afrOtqk6a7Fiy21ObP95TGujXwThuwQSjKcUzTdCbD94ERhleZLqnPYEpb6_Jcc1OBY3kUJvCjoUhXwbW6PhWr533JDEF' +
         'HoNCkPfhHS7vVCUFx4mQASkPLBud5arFSZU1uDStuiftJXfnQTWaMoJeA1N6rywB3xwLH__lHZQwEh4KnuYVuCeOMU1t8inuHI4EpZ4iTi2' +
         'LrL0Cl6HadpHv-GENYwuPDVq9qg2Mo75o1X6wpPSN1J5KUDAATyR_0hurg4A1DlVpVWtykP5YWEmx_g5w4MZfEVwH-JJjEhJRxLKajxrjfG' +
         '4XlnwwxPTznr1k1Mb7getKbLSbiMA3fvAgl1IjBIB8eFaISauFPpLPSpKHCVZrQYPIKSxMVdlXHgwm3CRrkR29GevCM5iSwRHQK6_HWfIQI' +
         'lJ8H7uVQqXkNMvNmFldnfi3dj-oY6wMhs1ffP4RAsb5UTljvhJc6GoygBL_b0rv4aKcywDF8wa2P6B8gGFl6cVvWBQmWLJ-HL5RR68J_Omv' +
         'JUm3PD-wwX3YigStd6thdTNlHOZhl4ysn8ulkFY3Rz2jEBV_nO6EXBLdOmxn_yX77qQ9yPcE64uC8iDTFWpQU1gmOF38od96oYD-T-whVl1' +
         'NLD2bOvFVdd4UmWpb3Ui8AYzKzFBHNczAogQfplFmr8VABsgtWk8hW8csam70NADWK54SZOPQHeiOt1Mb488OZiDpX0FifEnCac78C_5uEi' +
         'OPa8FAHpgUJ_XeXg83doDsAvrE1ZkrgFDwzT5pUTLyqh9eI1PAQKCoKmAbofcZM65o_qGvmnpN8fGVudOoHb0_Dqu_E2RBbaLcxsIe5jWmG' +
         'mth4sb_4ANLFCmtt8T8sDmOdcYtQmhpUzg6rjDqeU76yq6fC15bGjT6Qc-EAgrUftFVwLw2UIGAbHmeAyFbSuJiMaUDJeYoZ3zxoID_DRCP' +
         '9kN3ty-EQMHM9BZgXlH9dJ8ZUCAeH59h4PinM5LQuiS_kvP1iyT1Be6sYglV2dB7W_AziOcrrBiLfjazbUUpwqLm4_Yt_QwYJrAWfYyFOxk' +
         'KxkT5qi2c1RNGBtiYjv8X_TRJDg0uxX_Hbq0Pc7yYezFQdFYIlRAvJcnc8PiOfhWtQZpCIYKTskg_Y2UfVvjydXYcGuIA2700PzR9ga-1VR' +
         '7mv9UOLHe2x4ALiOO7Iz6KgOfVYMJ9dIC7f4HY9nrnLdhfKw5dcIf7RDhDqrkPyz8LLTuAuO-hGSwoP35XFkf0FQ8f1Cg5J_k-S3S2dCj8D' +
         'XIRLcEJ9Qb5zvDofIPSmNKlvwJkqplDLBWlAow'
  };

  if (hasOpenSSL(3, 5)) {
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, alg: 'ml-dsa-44' } }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87'/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, alg: undefined } }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'ML-DSA-44', 'ML-DSA-65', 'ML-DSA-87'/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, pub: undefined } }),
                  { code: 'ERR_INVALID_ARG_TYPE', message: /The "key\.pub" property must be of type string/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: undefined } }),
                  { code: 'ERR_INVALID_ARG_TYPE', message: /The "key\.priv" property must be of type string/ });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, priv: Buffer.alloc(33).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });
    assert.throws(() => createPrivateKey({ format, key: { ...jwk, pub: Buffer.alloc(1313).toString('base64url') } }),
                  { code: 'ERR_CRYPTO_INVALID_JWK' });

    assert.ok(createPrivateKey({ format, key: jwk }));
    assert.ok(createPublicKey({ format, key: jwk }));
  } else {
    assert.throws(() => createPrivateKey({ format, key: jwk }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'RSA', 'EC', 'OKP'\. Received 'AKP'/ });
    assert.throws(() => createPublicKey({ format, key: jwk }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'RSA', 'EC', 'OKP'\. Received 'AKP'/ });
  }
}
