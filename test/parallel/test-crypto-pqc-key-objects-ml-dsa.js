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
    // eslint-disable-next-line @stylistic/js/max-len
    pub: 'SXghXj9P-DJ5eznNa_zLJxRxpa0mt86WlIid0EVEv1qraLBkC1UKevSZrjtgo1QUEN0oa3tP-HyYj8Onnc1zEnxsSoeC5A-PgywKgYuZP581wGPS-cbA2-5acsg-YUi_9fDkLR5YOTQQ3Iu952K1m8w0QDIBxZjecm32HgkD56CCC6ZyBOwfx9qcNUeO0aImya1igzL2_LRsqomogl9OuduWhtussAavGlAK7ZR4_4lmyjWcdeIc-z--iy42biV5d_tnopfNTJFlycBKinZu3h0lr4-ldl6apGDIyvSdZulhgj_j6jgEX-AgQZgS93ctx680GvROkBL7YI_3iXW3REWzVgS9HLasagEi2h6-RYQ9RzgUTODbei5fNRj3bNSqr8IKTZ08DCsRasN61TGwE3F7meoauw2NYkV51mhTxIafwhLWJrRA4C09Y-afrOtqk6a7Fiy21ObP95TGujXwThuwQSjKcUzTdCbD94ERhleZLqnPYEpb6_Jcc1OBY3kUJvCjoUhXwbW6PhWr533JDEFHoNCkPfhHS7vVCUFx4mQASkPLBud5arFSZU1uDStuiftJXfnQTWaMoJeA1N6rywB3xwLH__lHZQwEh4KnuYVuCeOMU1t8inuHI4EpZ4iTi2LrL0Cl6HadpHv-GENYwuPDVq9qg2Mo75o1X6wpPSN1J5KUDAATyR_0hurg4A1DlVpVWtykP5YWEmx_g5w4MZfEVwH-JJjEhJRxLKajxrjfG4XlnwwxPTznr1k1Mb7getKbLSbiMA3fvAgl1IjBIB8eFaISauFPpLPSpKHCVZrQYPIKSxMVdlXHgwm3CRrkR29GevCM5iSwRHQK6_HWfIQIlJ8H7uVQqXkNMvNmFldnfi3dj-oY6wMhs1ffP4RAsb5UTljvhJc6GoygBL_b0rv4aKcywDF8wa2P6B8gGFl6cVvWBQmWLJ-HL5RR68J_OmvJUm3PD-wwX3YigStd6thdTNlHOZhl4ysn8ulkFY3Rz2jEBV_nO6EXBLdOmxn_yX77qQ9yPcE64uC8iDTFWpQU1gmOF38od96oYD-T-whVl1NLD2bOvFVdd4UmWpb3Ui8AYzKzFBHNczAogQfplFmr8VABsgtWk8hW8csam70NADWK54SZOPQHeiOt1Mb488OZiDpX0FifEnCac78C_5uEiOPa8FAHpgUJ_XeXg83doDsAvrE1ZkrgFDwzT5pUTLyqh9eI1PAQKCoKmAbofcZM65o_qGvmnpN8fGVudOoHb0_Dqu_E2RBbaLcxsIe5jWmGmth4sb_4ANLFCmtt8T8sDmOdcYtQmhpUzg6rjDqeU76yq6fC15bGjT6Qc-EAgrUftFVwLw2UIGAbHmeAyFbSuJiMaUDJeYoZ3zxoID_DRCP9kN3ty-EQMHM9BZgXlH9dJ8ZUCAeH59h4PinM5LQuiS_kvP1iyT1Be6sYglV2dB7W_AziOcrrBiLfjazbUUpwqLm4_Yt_QwYJrAWfYyFOxkKxkT5qi2c1RNGBtiYjv8X_TRJDg0uxX_Hbq0Pc7yYezFQdFYIlRAvJcnc8PiOfhWtQZpCIYKTskg_Y2UfVvjydXYcGuIA2700PzR9ga-1VR7mv9UOLHe2x4ALiOO7Iz6KgOfVYMJ9dIC7f4HY9nrnLdhfKw5dcIf7RDhDqrkPyz8LLTuAuO-hGSwoP35XFkf0FQ8f1Cg5J_k-S3S2dCj8DXIRLcEJ9Qb5zvDofIPSmNKlvwJkqplDLBWlAow'
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

    // Test vectors from ietf-cose-dilithium
    {
      for (const jwk of [
        {
          kty: 'AKP',
          alg: 'ML-DSA-44',
          // eslint-disable-next-line @stylistic/js/max-len
          pub: 'unH59k4RuutY-pxvu24U5h8YZD2rSVtHU5qRZsoBmBMcRPgmu9VuNOVdteXi1zNIXjnqJg_GAAxepLqA00Vc3lO0bzRIKu39VFD8Lhuk8l0V-cFEJC-zm7UihxiQMMUEmOFxe3x1ixkKZ0jqmqP3rKryx8tSbtcXyfea64QhT6XNje2SoMP6FViBDxLHBQo2dwjRls0k5a-XSQSu2OTOiHLoaWsLe8pQ5FLNfTDqmkrawDEdZyxr3oSWJAsHQxRjcIiVzZuvwxYy1zl2STiP2vy_fTBaPemkleynQzqPg7oPCyXEE8bjnJbrfWkbNNN8438e6tHPIX4l7zTuzz98YPhLjt_d6EBdT4MldsYe-Y4KLyjaGHcAlTkk9oa5RhRwW89T0z_t1DSO3dvfKLUGXh8gd1BD6Fz5MfgpF5NjoafnQEqDjsAAhrCXY4b-Y3yYJEdX4_dp3dRGdHG_rWcPmgX4JG7lCnser4f8QGnDriqiAzJYEXeS8LzUngg_0bx0lqv_KcyU5IaLISFO0xZSU5mmEPvdSoDnyAcV8pV44qhLtAvd29n0ehG259oRihtljTWeiu9V60a1N2tbZVl5mEqSK-6_xZvNYA1TCdzNctvweH24unV7U3wer9XA9Q6kvJWDVJ4oKaQsKMrCSMlteBJMRxWbGK7ddUq6F7GdQw-3j2M-qdJvVKm9UPjY9rc1lPgol25-oJxTu7nxGlbJUH-4m5pevAN6NyZ6lfhbjWTKlxkrEKZvQXs_Yf6cpXEwpI_ZJeriq1UC1XHIpRkDwdOY9MH3an4RdDl2r9vGl_IwlKPNdh_5aF3jLgn7PCit1FNJAwC8fIncAXgAlgcXIpRXdfJk4bBiO89GGccSyDh2EgXYdpG3XvNgGWy7npuSoNTE7WIyblAk13UQuO4sdCbMIuriCdyfE73mvwj15xgb07RZRQtFGlFTmnFcIdZ90zDrWXDbANntv7KCKwNvoTuv64bY3HiGbj-NQ-U9eMylWVpvr4hrXcES8c9K3PqHWADZC0iIOvlzFv4VBoc_wVflcOrL_SIoaNFCNBAZZq-2v5lAgpJTqVOtqJ_HVraoSfcKy5g45p-qULunXj6Jwq21fobQiKubBKKOZwcJFyJD7F4ACKXOrz-HIvSHMCWW_9dVrRuCpJw0s0aVFbRqopDNhu446nqb4_EDYQM1tTHMozPd_jKxRRD0sH75X8ZoToxFSpLBDbtdWcenxj-zBf6IGWfZnmaetjKEBYJWC7QDQx1A91pJVJCEgieCkoIfTqkeQuePpIyu48g2FG3P1zjRF-kumhUTfSjo5qS0YiZQy0E1BMs6M11EvuxXRsHClLHoy5nLYI2Sj4zjVjYyxSHyPRPGGo9hwB34yWxzYNtPPGiqXS_dNCpi_zRZwRY4lCGrQ-hYTEWIK1Dm5OlttvC4_eiQ1dv63NiGkLRJ5kJA3bICN0fzCDY-MBqnd1cWn8YVBijVkgtaoascjL9EywDgJdeHnXK0eeOvUxHHhXJVkNqcibn8O4RQdpVU60TSA-uiu675ytIjcBHC6kTv8A8pmkj_4oypPd-F92YIJC741swkYQoeIHj8rE-ThcMUkF7KqC5VORbZTRp8HsZSqgiJcIPaouuxd1-8Rxrid3fXkE6p8bkrysPYoxWEJgh7ZFsRCPDWX-yTeJwFN0PKFP1j0F6YtlLfK5wv-c4F8ZQHA_-yc_gODicy7KmWDZgbTP07e7gEWzw4MFRrndjbDQ',
          priv: 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'
        },
        {
          kty: 'AKP',
          alg: 'ML-DSA-65',
          // eslint-disable-next-line @stylistic/js/max-len
          pub: 'QksvJn5Y1bO0TXGs_Gpla7JpUNV8YdsciAvPof6rRD8JQquL2619cIq7w1YHj22ZolInH-YsdAkeuUr7m5JkxQqIjg3-2AzV-yy9NmfmDVOevkSTAhnNT67RXbs0VaJkgCufSbzkLudVD-_91GQqVa3mk4aKRgy-wD9PyZpOMLzP-opHXlOVOWZ067galJN1h4gPbb0nvxxPWp7kPN2LDlOzt_tJxzrfvC1PjFQwNSDCm_l-Ju5X2zQtlXyJOTZSLQlCtB2C7jdyoAVwrftUXBFDkisElvgmoKlwBks23fU0tfjhwc0LVWXqhGtFQx8GGBQ-zol3e7P2EXmtIClf4KbgYq5u7Lwu848qwaItyTt7EmM2IjxVth64wHlVQruy3GXnIurcaGb_qWg764qZmteoPl5uAWwuTDX292Sa071S7GfsHFxue5lydxIYvpVUu6dyfwuExEubCovYMfz_LJd5zNTKMMatdbBJg-Qd6JPuXznqc1UYC3CccEXCLTOgg_auB6EUdG0b_cy-5bkEOHm7Wi4SDipGNig_ShzUkkot5qSqPZnd2I9IqqToi_0ep2nYLBB3ny3teW21Qpccoom3aGPt5Zl7fpzhg7Q8zsJ4sQ2SuHRCzgQ1uxYlFx21VUtHAjnFDSoMOkGyo4gH2wcLR7-z59EPPNl51pljyNefgCnMSkjrBPyz1wiET-uqi23f8Bq2TVk1jmUFxOwdfLsU7SIS30WOzvwD_gMDexUFpMlEQyL1-Y36kaTLjEWGCi2tx1FTULttQx5JpryPW6lW5oKw5RMyGpfRliYCiRyQePYqipZGoxOHpvCWhCZIN4meDY7H0RxWWQEpiyCzRQgWkOtMViwao6Jb7wZWbLNMebwLJeQJXWunk-gTEeQaMykVJobwDUiX-E_E7fSybVRTZXherY1jrvZKh8C5Gi5VADg5Vs319uN8-dVILRyOOlvjjxclmsRcn6HEvTvxd9MS7lKm2gI8BXIqhzgnTdqNGwTpmDHPV8hygqJWxWXCltBSSgY6OkGkioMAmXjZjYq_Ya9o6AE7WU_hUdm-wZmQLExwtJWEIBdDxrUxA9L9JL3weNyQtaGItPjXcheZiNBBbJTUxXwIYLnXtT1M0mHzMqGFFWXVKsN_AIdHyv4yDzY9m-tuQRfbQ_2K7r5eDOL1Tj8DZ-s8yXG74MMBqOUvlglJNgNcbuPKLRPbSDoN0E3BYkfeDgiUrXy34a5-vU-PkAWCsgAh539wJUUBxqw90V1Du7eTHFKDJEMSFYwusbPhEX4ZTwoeTHg--8Ysn4HCFWLQ00pfBCteqvMvMflcWwVfTnogcPsJb1bEFVSc3nTzhk6Ln8J-MplyS0Y5mGBEtVko_WlyeFsoDCWj4hqrgU7L-ww8vsCRSQfskH8lodiLzj0xmugiKjWUXbYq98x1zSnB9dmPy5P3UNwwMQdpebtR38N9I-jup4Bzok0-JsaOe7EORZ8ld7kAgDWa4K7BAxjc2eD540Apwxs-VLGFVkXbQgYYeDNG2tW1Xt20-XezJqZVUl6-IZXsqc7DijwNInO3fT5o8ZAcLKUUlzSlEXe8sIlHaxjLoJ-oubRtlKKUbzWOHeyxmYZSxYqQhSQj4sheedGXJEYWJ-Y5DRqB-xpy-cftxL10fdXIUhe1hWFBAoQU3b5xRY8KCytYnfLhsFF4O49xhnax3vuumLpJbCqTXpLureoKg5PvWfnpFPB0P-ZWQN35mBzqbb3ZV6U0rU55DvyXTuiZOK2Z1TxbaAd1OZMmg0cpuzewgueV-Nh_UubIqNto5RXCd7vqgqdXDUKAiWyYegYIkD4wbGMqIjxV8Oo2ggOcSj9UQPS1rD5u0rLckAzsxyty9Q5JsmKa0w8Eh7Jwe4Yob4xPVWWbJfm916avRgzDxXo5gmY7txdGFYHhlolJKdhBU9h6f0gtKEtbiUzhp4IWsqAR8riHQs7lLVEz6P537a4kL1r5FjfDf_yjJDBQmy_kdWMDqaNln-MlKK8eENjUO-qZGy0Ql4bMZtNbHXjfJUuSzapA-RqYfkqSLKgQUOW8NTDKhUk73yqCU3TQqDEKaGAoTsPscyMm7u_8QrvUK8kbc-XnxrWZ0BZJBjdinzh2w-QvjbWQ5mqFp4OMgY94__tIU8vvCUNJiYA1RdyodlfPfH5-avpxOCvBD6C7ZIDyQ-6huGEQEAb6DP8ydWIZQ8xY603DoEKKXkJWcP6CJo3nHFEdj_vcEbDQ-WESDpcQFa1fRIiGuALj-sEWcjGdSHyE8QATOcuWl4TLVzRPKAf4tCXx1zyvhJbXQu0jf0yfzVpOhPun4n-xqK4SxPBCeuJOkQ2VG9jDXWH4pnjbAcrqjveJqVti7huMXTLGuqU2uoihBw6mGqu_WSlOP2-XTEyRyvxbv2t-z9V6GPt1V9ceBukA0oGwtJqgD-q7NXFK8zhw7desI5PZMXf3nuVgbJ3xdvAlzkmm5f9RoqQS6_hqwPQEcclq1MEZ3yML5hc99TDtZWy9gGkhR0Hs3QJxxgP7bEqGFP-HjTPnJsrGaT6TjKP7qCxJlcFKLUr5AU_kxMULeUysWWtSGJ9mpxBvsyW1Juo',
          priv: 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'
        },
        {
          kty: 'AKP',
          alg: 'ML-DSA-87',
          // eslint-disable-next-line @stylistic/js/max-len
          pub: '5F_8jMc9uIXcZi5ioYzY44AylxF_pWWIFKmFtf8dt7Roz8gruSnx2Gt37RT1rhamU2h3LOUZEkEBBeBFaXWukf22Q7US8STV5gvWi4x-Mf4Bx7DcZa5HBQHMVlpuHfz8_RJWVDPEr-3VEYIeLpYQxFJ14oNt7jXO1p1--mcv0eQxi-9etuiX6LRRqiAt7QQrKq73envj9pkUbaIpqL2z_6SWRFln51IXv7yQSPmVZEPYcx-DPrMN4Q2slv_-fPZeoERcPjHoYB4TO-ahAHZP4xluJncmRB8xdR-_mm9YgGRPTnJ15X3isPEF5NsFXVDdHJyTT931NbjeKLDHTARJ8iLNLtC7j7x3XM7oyUBmW0D3EvT34AdQ6eHkzZz_JdGUXD6bylPM1PEu7nWBhW69aPJoRZVuPnvrdh8P51vdMb_i-gGBEzl7OHvVnWKmi4r3-iRauTLmn3eOLO79ITBPu4CZ6hPY6lfBgTGXovda4lEHW1Ha04-FNmnp1fmKNlUJiUGZOhWUhg-6cf5TDuXCn1jyl4r2iMy3Wlg4o1nBEumOJahYOsjawfhh_Vjir7pd5aUuAgkE9bQrwIdONb788-YRloR2jzbgCPBHEhd86-YnYHOB5W6q7hYcFym43lHb3kdNSMxoJJ6icWK4eZPmDITtbMZCPLNnbZ61CyyrWjoEnvExOB1iP6b7y8nbHnzAJeoEGLna0sxszU6V-izsJP7spwMYp1Fxa3IT9j7b9lpjM4NX-Dj5TsBxgiwkhRJIiFEHs9HE6SRnjHYU6hrwOBBGGfKuNylAvs-mninLtf9sPiCke-Sk90usNMEzwApqcGrMxv_T2OT71pqZcE4Sg8hQ2MWNHldTzZWHuDxMNGy5pYE3IT7BCDTGat_iu1xQGo7y7K3Rtnej3xpt64br8HIsT1Aw4g-QGN1bb8U-6iT9kre1tAJf6umW0-SP1MZQ2C261-r5NmOWmFEvJiU9LvaEfIUY6FZcyaVJXG__V83nMjiCxUp9tHCrLa-P_Sv3lPp8aS2ef71TLuzB14gOLKCzIWEovii0qfHRUfrJeAiwvZi3tDphKprIZYEr_qxvR0YCd4QLUqOwh_kWynztwPdo6ivRnqIRVfhLSgTEAArSrgWHFU1WC8Ckd6T5MpqJhN0x6x8qBePZGHAdYwz8qa9h7wiNLFWBrLRj5DmQLl1CVxnpVrjW33MFso4P8n060N4ghdKSSZsZozkNQ5b7O6yajYy-rSp6QpD8msb8oEX5imFKRaOcviQ2D4TRT45HJxKs63Tb9FtT1JoORzfkdv_E1bL3zSR6oYbTt2Stnpz-7kVqc8KR2N45EkFKxDkRw3IXOte0cq81xoU87S_ntf4KiVZaszuqb2XN2SgxnXBl4EDnpehPmqkD92SAlLrQcTaxaSe47G28K-8MwoVt4eeVkj4UEsSfJN7rbCH2yKl2XJx5huDaS0xn2ODQyNRmgk-5I9hXMUiZDNLvEzx4zuyrcu2d0oXFo3ZoUtVFNCB__TQCf2x27ej9GjLXLDAEi7qnl9Xfb94n0IfeVyGte3-j6NP3DWv8OrLiUjNTaLv6Fay1yzfUaU6LI86-Jd6ckloiGhg7kE0_hd-ZKakZxU1vh0Vzc6DW7MFAPky75iCZlDXoBpZjTNGo5HR-mCW_ozblu60U9zZA8bn-voANuu_hYwxh-uY1sHTFZOqp2xicnnMChz_GTm1Je8XCkICYegeiHUryEHA6T6B_L9gW8S_R4ptMD0Sv6b1KHqqKeubwKltCWPUsr2En9iYypnz06DEL5Wp8KMhrLid2AMPpLI0j1CWGJExXHpBWjfIC8vbYH4YKVl-euRo8eDcuKosb5hxUGM9Jvy1siVXUpIKpkZt2YLP5pEBP_EVOoHPh5LJomrLMpORr1wBKbEkfom7npX1g817bK4IeYmZELI8zXUUtUkx3LgNTckwjx90Vt6oVXpFEICIUDF_LAVMUftzz6JUvbwOZo8iAZqcnVslAmRXeY_ZPp5eEHFfHlsb8VQ73Rd_p8XlFf5R1WuWiUGp2TzJ-VQvj3BTdQfOwSxR9RUk4xjqNabLqTFcQ7As246bHJXH6XVnd4DbEIDPfNa8FaWb_DNEgQAiXGqa6n7l7aFq5_6Kp0XeBBM0sOzJt4fy8JC6U0DEcMnWxKFDtMM7q06LubQYFCEEdQ5b1Qh2LbQZ898tegmeF--EZ4F4hvYebZPV8sM0ZcsKBXyCr585qs00PRxr0S6rReekGRBIvXzMojmid3dxc6DPpdV3x5zxlxaIBxO3i_6axknSSdxnS04_bemWqQ3CLf6mpSqfTIQJT1407GB4QINAAC9Ch3AXUR_n1jr64TGWzbIr8uDcnoVCJlOgmlXpmOwubigAzJattbWRi7k4QYBnA3_4QMjt73n2Co4-F_Qh4boYLpmwWG2SwcIw2PeXGr2LY2zwkPR4bcSyx1Z6UK5trQpWlpQCxgsvV_RvGzpN22RtHoihPH74K0cBIzCz7tK-jqeuWl1A7af7KmQ66fpRBr5ykTLOsa17WblkcIB_jDvqKfEcdxhPWJUwmOo4TIQS-xH8arLOy_NQFG2m14_yxwUemXC-QxLUYi6_FIcqwPBKjCdpQtadRdyftQSKO0SP-GxUvamMZzWI780rXuOBkq5kyYLy9QF9bf_-bL6QLpe1WMCQlOeXZaCPoncgYoT0WZ17jB52Xb2lPWsyXYK54npszkbKJ4OIqfvF8xqRXcVe22VwJuqT9Uy4-4KKQgQ7TXla7Gdm2H7mKl8YXQlsGCT2Ypc8O4t0Sfw7qYAuaDGf752Hbm3fl1bupcB2huIPlIaDP6IRR9XvTYIW2flbwYfhKLmoVKnG85uUi2qtqCjPOIuU3-peT0othfmwKQXaoOqO-V4r6wPL1VHxVFtIYmEdVt0RccUOvpOVR_OAHG9uHOzTmueK5557Qxp0ojtZCHyN-hgoMZJLrvdKkTCxPNo2-mZQbHoVh2FnThZ9JbO49dB8lKXP4_MU5xAnjXMgKXtbfI8w6ZWATE_XWgf2VQMUpGp4wpy44yWQTxHxh_4T9540BGwG0FU0bkgrwA_erseGZnepqdmz5_ScCs84O5Xr5MbYhJLCGGxY6O5GqS-ooB2w0Mt87KbbE4bpYje9CAHH8FX3pDrJyLsyasA3zxmk4OmGpG7Z70ofONJtHRe56R5287vFmuazEEutXn81kNzB-3aJT1ga3vnWZw4CSvFKoWYSA7auLgrHSHFZdITfOrgtmQmGbFhM9kSBdY1UCnpzf65oos3PZWRa2twfUxxLAnPNtrxpRGyvtsapw7ljUagZmuyh3hLCjhAxYmnoE1dbyIWvpCqSlEtVjL1yb_nuLEzgvmZuV02fHxGuWgHTOMVGXpf81Rce3eoBK3lapW1wkzezlk3tcA2bZOtA9qbxdsbVR37kemzQ9K1e3Y0OWhtSj',
          priv: 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'
        },
      ]) {
        assert.partialDeepStrictEqual(jwk, createPublicKey({ format, key: jwk }).export({ format: 'jwk' }));
        assert.deepStrictEqual(createPrivateKey({ format, key: jwk }).export({ format: 'jwk' }), jwk);
      }

    }
  } else {
    assert.throws(() => createPrivateKey({ format, key: jwk }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'RSA', 'EC', 'OKP'\. Received 'AKP'/ });
    assert.throws(() => createPublicKey({ format, key: jwk }),
                  { code: 'ERR_INVALID_ARG_VALUE', message: /must be one of: 'RSA', 'EC', 'OKP'\. Received 'AKP'/ });
  }
}
