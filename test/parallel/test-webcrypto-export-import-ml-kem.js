'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const fixtures = require('../common/fixtures');

function getKeyFileName(type, suffix) {
  return `${type.replaceAll('-', '_')}_${suffix}.pem`;
}

function toDer(pem) {
  const der = pem.replace(/(?:-----(?:BEGIN|END) (?:PRIVATE|PUBLIC) KEY-----|\s)/g, '');
  return Buffer.alloc(Buffer.byteLength(der, 'base64'), der, 'base64');
}

/* eslint-disable @stylistic/js/max-len */
const keyData = {
  'ML-KEM-512': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-kem-512', 'public'), 'ascii')),
    pub_len: 800,
    jwk: {
      kty: 'AKP',
      alg: 'ML-KEM-512',
      pub: '2yWZ7mEhbKJopHmZYXjGERQrHBHCQqBThjwVU5gvTPWqPuqAo6cHWrOrHCBt9aGSyPO2BrM0w7O1UZJsadMmhRJaOvN42AvD6OuebvYke_ITOyi2rbE_zww648ox1JF8ZMsTR8ppj_qX_qiAYaFCzpqGG5x_swuYBshuwnSZ2pilOTQ577dcOHNUDUZ5tjdPecuxkpQ_QOhhGJHKzoVY1Yiw4zpPD9kKLxO7BzUbTLcNZSs5VBJ5Ukawy4sfjQa4QNRooMIVCgG7IbVYT-yvgXljdEGxfBYpsewdHXWCF7EHtEmlYsIp3xlPjSbE6FFLQLdqQsiI5qotm4JGPCGvlQsi1AHDyaPC1ZImL4hDG8IXsPxPfwfMS7On5Atn98wxMQo9VSizpps7LFfOMYRH6FmYljbP9BlWesYxGXkZ1DRza0tKrPaZMjAkoRSrGbsHahwdyNtMVnirVyS3OwWmHHSHOyNfwKo2WWVLK2u8pxiVi0d7BCEx7mW4otGhL_iPn8sjieYWGRvJ7Ly0U8areAsFe3KbuHudnAMFw6A38HsIWbbFUjkFb7IJv4WONuUoCqpDknOYjBKUW2U-qtafz-OrLDbPOtBC0iDGTjx90sLHG7WgCbuwfOS1GVcjwNIctqxjXLB-VsEGXOmdzXvJ69MllEB9asBc1AtA86JH8fVMyAYW1xsEbDO6KspBcEtcusJevSKSFGgg0TxrJWWZXtyZ83XBLqVn2AG6WcqE46Zx5tNhrRY7bSIVLPALa6fLPHxmO0CG0lCTlDi-ZwB_GUB7xYBsTcigoIBtNdg_BlqAEVG67jm0zlg3KDMNbyfCBVEpWjAxq4mm5DjMcFaYaXiUv0RnamoSMmG5TXU6HbIC3ZcXs8FX-5N-susyj0wrKkyJI6vCvIOmAnGIvEQj5rc-0qRZZUXIiOdj2rBYk-nAYvqw6pt9h9FVS2g4siaHIZRsjzo5R0h0atWP0ckqTOe13XGM07kvS4wjjYMgawcAfNCUPnR-uMdsy5wuGDx-FWb6gRnhWsPg1S7W-B2grxSPIwotTjc1sKB3eUq0pa4',
      priv: 'sjVhZaMDzdhH7rMaw7Z9HH-7LfipY2weXpCqr0I3CMzOt_jB1xtlVqz7oLTcOAx1JNA-E7yXLnGPgZdplLmOaA',
    },
  },
  'ML-KEM-768': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-kem-768', 'public'), 'ascii')),
    pub_len: 1184,
    jwk: {
      kty: 'AKP',
      alg: 'ML-KEM-768',
      pub: 'z9U_gZgaizCOmrqIThIwjRqvJlO8UoFKVrJ3hOQ-Unq9jMKHMJwxsGzDLOAyu-aVbQtJ4slhn8KtXUfF16ln2dxF2cvJDlUHQrrLKLipMKAx5KJh8YMwMhJzUwoRySHGitCikgRDMQozJ-SLmiZd4UY6WfaRL2RHYNkRWxS9I7iIegW3LFc_r-UqP3uew6dcXQXKYOjMg1u9NYg_DLO6f6vB80QnxJI9iMezKmiQHaSokWFuTsYzzHQcbsIg6YoCGvSQQnShzhRJJdhUFxoTZmxZABaGJ1yW0El7N_BWqLBlAVPGbJyGKvOQ4Pos0jGM79Gs_UObJfRykTFK6Ti9fGATnVS98lJu9fkbCYGZTsebqvFED6Ge-QcMu5NTtGiGsDyDY6iqNsS1rqfA-kJ-SaoWDuocAUFqkRHG1nVmEZG4xFZnAjlsO2duiDvD7CrE6CNfVRaapyZh8QbPXmVe5EQH0jEZONc6I7bOeLaznSrCcty1xoF2tCigxOQMbrxlnSanrZgisBpsrxVWpiQD3OXDw4e9njWgiEJhhPcFJpWYS1W0SuUoVJkUT6yR-puk_ENq92Ce6XWFirVwHyWmGCai-acl1_KShFJC_AcuHVQLWACJGxyIVSwQEpMTh1B8sjR6wVafCJKk0eTNuqQwCgd9m7UKK-y61UxpN9RrxffBnKlchpMJkaoimIxUhEEpT1OLh2Zo0QIeYwWn52JxDOh7FlGPZ4N8axsEbzVKAxMuiHWxH-ZJTaWZUzjK-VmW-WmMdJCfCaF63DwGLTYdWiBh3IYqKREx61sJT8F3CsknL_mMLLlHn2Z7L1lztfVtOXckoYR5mwN1ybwnkrxJ_pOqv8GgblEU1GQnBjHGBdaE8sGsqFmDn_rPSmqL2Lpmv_e41LQj7Gxb8XNMtAqDoOM0CBF4KUcPmjm-XjZSDJOFVxoc7NcYWkWiybmW9RSTgVAdzYkep7CAihqMD_R0eiGqehWC1ZTB5_mkJhstbWyjJ7Qs6bdlzqy_hWdeZeQy9sGxnNyF_lik2qfGAKTLwSVhJzBnj2pngBKiYUmq8Mxl2Pg3xjVX73ys51gXX3fES1LCWuXGVtx9dblnYSA2LOCZqqZwZXWPNWCkEvSYQywHM-RwN2dan4F7v1O53bcD2iLGOxXO9-m7bMB7txwpBRdS5aLD9VJjYCmnymcXGOQX9tSdstddX7o5UfB6xrZeXKg-FMyYtnAzM8eJ2vZn1-IYyWoilrzPxftCf2uVU9w2BLZmLCdHvPRPtInFJ8S802KDBvJhJrUAmHgoI9uorjSwOboDpjrIANpoi_t7IpoiS1RAkofDwTxy_wcXt2kBQ1aHSoBhukaIjZZCBwhlE3E-rcaLptUfwGkg3XULcLuxg3F-GSNYWVAffuQS-oUnIrZOmCrCT3xk4tIzy4QX2PlDpuoYjmCsJQQ6o6gfmTYNZzfIZ-hEoxwfFeGyDNZZj-pygbpaOiY2t1dyE0I6PMp3mUKsHMkaXplYoYG7sLy1RvjAISFkmXyC6zmCh5k3QhB7YfygA-qz0Z3cUbIfezcJdafgaqMyR8tSsGyKJO0',
      priv: 'kTfye65c20Ez4T1rDv5YsC_kLexfNxNQHRZohALP3OhhjjiwFoBsHg0-HRLIWO3Nx5OwGhsUToPjfW0OWIU34A',
    },
  },
  'ML-KEM-1024': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-kem-1024', 'public'), 'ascii')),
    pub_len: 1568,
    jwk: {
      kty: 'AKP',
      alg: 'ML-KEM-1024',
      pub: 'DdeBJ4eqvZcmMWheoVWLNhEF81a4GZCrE3Zc2aNgyvY0adJyciCEsFsEvfy3WSXAuRkChxyZtale1ZC0RcVXouFbHEt6mAimzfgaXAojEkVylpOIOppPUZkV8IRiZ4IutPurDNqRXdY3fQmYWwExVSFfoSFDgTgnCUvKvTk79yQoNQucORZ9HFwPj_K7-TpyGbgC7GElMtui4rBHSny0BCJMZeafG8wqK9BcXRN06SmxqRNqsRx9HLJNwphr7IFcMlZTCIxrxoMckfkMe0BiVSAjVNaIB5RvciJbcmR2xqZPEYNTVVKAuIyaJvmLHQEv-ObKfwUnz6WN0NGZjhp7_KYAxYowXjpuThMGGuuSbVyGh8JYzCMx2ng_T0ZxeDG_lAcmHKRTpcdzNThXnsuh9YVzD6aQpyCcaYqxeGVQoex-euQEyPc6PqcsyfHLWxlgPuoCx4p_BAunJCASz_tjClUheiQcCaYAmMmSylOElQwsovdX9uy_NOJ9BmY9cDSMPnaJhFhvIxFR1jNg09ZCuXRYJDiIyWBK2czA3vt9ZVo9o1QnM1SZjvYyCoBO23LHGSZKuHw_ISOeTcMv76qE_4kNlpXLktBxxXmPNBJ7rZC_xaemvkGS8zjGX2Bqf7cGDzKQbEhWk3ovptV1FmN2XykR7DkEa7pOn6iKpCB5yCOr4AHM0ViKqoa7Z3MysSinTianEKVsVLBH0TwKt4c8g0ihCQaITTOtF2QtjYMSIGzHnqVNacJ8yNxDRVLBOqY7G2qXLTp8YUuZ2fCXevBcCQYQNeYQlWURtPMvtiIvpIiIZgc732fFGThZ38bH4xG5KRxD1xq1gLWQZwkYFvhoFSNsVSpfjIJJSaPD76nLk_KfAlQMpQY0SXCzjKGVeoehT7eHrwpQy-oZoThCmYZTI8E5khTCk6h8fis8WAwKnTKLVVuHL2BVDZVG34lp3uR_bUGNDtgNQjJW1LctC6awz0nAtsZUVukbs4VM_Mt-kzZBPtoDPdTDZKGGGiQ3UJAuzisNgdomiloOFlWDWAmSvcYE_qAVnVw66LchMtkGZmEGSCWtIWpxQlNDNvY0nqwGUhzH8ruqlsti30jKu8yE8_J2RdkPKoQre3JcQlZnWaLOvoJ9nhplc7UgG_e229x1XgiTSSy_vLTFPWg_opM6ZhGDwMZwFMORgcExj2HEnlkyVImZeKAtU-GRCgCBKFPLJiNeLSCfNFSGu1NFoBYPxkOBCrUCX5lZt5Um1rYpeemR5otK7Kh_tGJIQUlogeMJTIGS7dZLz0EimmGaaeXAwhN5JPBXkiKPhVhXX4tYb3uaSYsQbtRiw0RdEhysp6EyUrolf-OqOEwbiLJoJiN3hgcP-tuX8ZXIdhNmmyMzD4lIdpZmkkAxD9M64CkK6YFDthCAkHt2E6qgB2liwiwbXNdqU7gH99V2wlycYrKMaKJU5xyzyaQMMUMscapGB8GGRKNhVJwzzka2AxG2tKo-mhllI8ZrcQuHOMQo21jJYkob6xgOhFulAlZ-_icTQGdZ4-g6pzk40ih9rDWfxUYyJNN67-g50jvLIOJrJQEYjcg4TXDBuFahMtTNqOpvAazHFDeqV1iQz0mHBpMMPvGi-Gs5GvBG4kK8m9dMXFtUHvRbToemW2jAzXFjzmW0BEkWs-WMvbwBL3p8mElHZzuNZCsp5EpEzIqDsgktvLHCa5IVN5CrPUzJC9VnMYYrDXIBF4yOLmtFxQmxQdF8RUJc0Oy1aKCA0bchkiuLIiW35tNMd6wDFJctpLoMGxIlNthoWTYYYeFNAEepvZdO_iM4NBd5lACS9pqyuvVx5GmGpFSsPHlyuMWMHQuyH5tf5tCMbgTCkFsvAxo0attaSVkYK6atSBlS8XB0pLQqd_cj4agOy5YSLbACpsI2KfJ1epKFgyB27_yVf7aa4xQt8ksu-ymrgBufVjg92pNzG2dSC0WZR4smS_RmTxZJjvBTVBbNwrilplZROHtWjjkIBNyzNbNu7Ds1c-Jo7WA39WE0awyYc4EVITG1khNS5oULg-AXvXTtwXmUn7GtGxXtlfBpE37RJhm21PYl7WQppG3c0Pk',
      priv: 'XwegslUGcMnzu1c1NccsyRmBaObWSESIGO-ftccj5MvAwjLDAJV_u5KhD_rnJLoLOu0eNGt4p3Oh6Mg5ttxICw',
    },
  },
};
/* eslint-enable @stylistic/js/max-len */

const testVectors = [
  {
    name: 'ML-KEM-512',
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    publicUsages: ['encapsulateKey', 'encapsulateBits']
  },
  {
    name: 'ML-KEM-768',
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    publicUsages: ['encapsulateKey', 'encapsulateBits']
  },
  {
    name: 'ML-KEM-1024',
    privateUsages: ['decapsulateKey', 'decapsulateBits'],
    publicUsages: ['encapsulateKey', 'encapsulateBits']
  },
];

async function testImportSpki({ name, publicUsages }, extractable) {
  const key = await subtle.importKey(
    'spki',
    keyData[name].spki,
    { name },
    extractable,
    publicUsages);
  assert.strictEqual(key.type, 'public');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, publicUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    // Test the roundtrip
    const spki = await subtle.exportKey('spki', key);
    assert.strictEqual(
      Buffer.from(spki).toString('hex'),
      keyData[name].spki.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('spki', key), {
        message: /key is not extractable/,
        name: 'InvalidAccessError',
      });
  }

  // Bad usage
  await assert.rejects(
    subtle.importKey(
      'spki',
      keyData[name].spki,
      { name },
      extractable,
      ['wrapKey']),
    { message: /Unsupported key usage/ });
}

async function testImportPkcs8({ name, privateUsages }, extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[name].pkcs8,
    { name },
    extractable,
    privateUsages);
  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    // Test the roundtrip
    const pkcs8 = await subtle.exportKey('pkcs8', key);
    assert.strictEqual(
      Buffer.from(pkcs8).toString('hex'),
      keyData[name].pkcs8_seed_only.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/,
        name: 'InvalidAccessError',
      });
  }

  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8,
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });
}

async function testImportPkcs8SeedOnly({ name, privateUsages }, extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[name].pkcs8_seed_only,
    { name },
    extractable,
    privateUsages);
  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    // Test the roundtrip
    const pkcs8 = await subtle.exportKey('pkcs8', key);
    assert.strictEqual(
      Buffer.from(pkcs8).toString('hex'),
      keyData[name].pkcs8_seed_only.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/,
        name: 'InvalidAccessError',
      });
  }

  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8_seed_only,
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });
}

async function testImportPkcs8PrivOnly({ name, privateUsages }, extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[name].pkcs8_priv_only,
    { name },
    extractable,
    privateUsages);
  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.deepStrictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);

  if (extractable) {
    await assert.rejects(subtle.exportKey('pkcs8', key), (err) => {
      assert.strictEqual(err.name, 'OperationError');
      assert.strictEqual(err.cause.code, 'ERR_CRYPTO_OPERATION_FAILED');
      assert.strictEqual(err.cause.message, 'Failed to get raw seed');
      return true;
    });
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/,
        name: 'InvalidAccessError',
      });
  }

  await assert.rejects(
    subtle.importKey(
      'pkcs8',
      keyData[name].pkcs8_seed_only,
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });
}

async function testImportRawPublic({ name, publicUsages }, extractable) {
  const pub = keyData[name].spki.subarray(-keyData[name].pub_len);

  const publicKey = await subtle.importKey(
    'raw-public',
    pub,
    { name },
    extractable, publicUsages);

  assert.strictEqual(publicKey.type, 'public');
  assert.deepStrictEqual(publicKey.usages, publicUsages);
  assert.strictEqual(publicKey.algorithm.name, name);
  assert.strictEqual(publicKey.algorithm, publicKey.algorithm);
  assert.strictEqual(publicKey.usages, publicKey.usages);
  assert.strictEqual(publicKey.extractable, extractable);

  if (extractable) {
    const value = await subtle.exportKey('raw-public', publicKey);
    assert.deepStrictEqual(Buffer.from(value), pub);

    await assert.rejects(subtle.exportKey('raw', publicKey), {
      name: 'NotSupportedError',
      message: `Unable to export ${publicKey.algorithm.name} public key using raw format`,
    });
  }

  await assert.rejects(
    subtle.importKey(
      'raw-public',
      pub.subarray(0, pub.byteLength - 1),
      { name },
      extractable, publicUsages),
    { message: 'Invalid keyData' });

  await assert.rejects(
    subtle.importKey(
      'raw-public',
      pub,
      { name: name === 'ML-KEM-512' ? 'ML-KEM-768' : 'ML-KEM-512' },
      extractable, publicUsages),
    { message: 'Invalid keyData' });
}

async function testImportRawSeed({ name, privateUsages }, extractable) {
  const seed = keyData[name].pkcs8_seed_only.subarray(-64);

  const privateKey = await subtle.importKey(
    'raw-seed',
    seed,
    { name },
    extractable, privateUsages);

  assert.strictEqual(privateKey.type, 'private');
  assert.deepStrictEqual(privateKey.usages, privateUsages);
  assert.strictEqual(privateKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm, privateKey.algorithm);
  assert.strictEqual(privateKey.usages, privateKey.usages);
  assert.strictEqual(privateKey.extractable, extractable);

  if (extractable) {
    const value = await subtle.exportKey('raw-seed', privateKey);
    assert.deepStrictEqual(Buffer.from(value), seed);
  }

  await assert.rejects(
    subtle.importKey(
      'raw-seed',
      seed.subarray(0, 30),
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid keyData' });
}

async function testImportJwk({ name, publicUsages, privateUsages }, extractable) {
  const jwk = keyData[name].jwk;

  const [
    publicKey,
    privateKey,
  ] = await Promise.all([
    subtle.importKey(
      'jwk',
      {
        kty: jwk.kty,
        alg: jwk.alg,
        pub: jwk.pub,
      },
      { name },
      extractable,
      publicUsages),
    subtle.importKey(
      'jwk',
      { ...jwk },
      { name },
      extractable,
      privateUsages),
  ]);

  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(publicKey.extractable, extractable);
  assert.strictEqual(privateKey.extractable, extractable);
  assert.strictEqual(publicKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm, privateKey.algorithm);
  assert.strictEqual(privateKey.usages, privateKey.usages);
  assert.strictEqual(publicKey.algorithm, publicKey.algorithm);
  assert.strictEqual(publicKey.usages, publicKey.usages);

  if (extractable) {
    const [
      pubJwk,
      pvtJwk,
    ] = await Promise.all([
      subtle.exportKey('jwk', publicKey),
      subtle.exportKey('jwk', privateKey),
    ]);

    assert.strictEqual(pubJwk.kty, 'AKP');
    assert.strictEqual(pvtJwk.kty, 'AKP');
    assert.strictEqual(pubJwk.alg, name);
    assert.strictEqual(pvtJwk.alg, name);
    assert.strictEqual(pubJwk.pub, jwk.pub);
    assert.strictEqual(pvtJwk.pub, jwk.pub);
    assert.strictEqual(pvtJwk.priv, jwk.priv);
    assert.strictEqual(pubJwk.priv, undefined);
  } else {
    await assert.rejects(
      subtle.exportKey('jwk', publicKey), {
        message: /key is not extractable/
      });
    await assert.rejects(
      subtle.exportKey('jwk', privateKey), {
        message: /key is not extractable/
      });
  }

  // Invalid use parameter
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { kty: jwk.kty, alg: jwk.alg, pub: jwk.pub, use: 'sig' },
      { name },
      extractable,
      publicUsages),
    { message: 'Invalid JWK "use" Parameter' });
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, use: 'sig' },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK "use" Parameter' });

  // Algorithm mismatch
  const wrongAlg = name === 'ML-KEM-512' ? 'ML-KEM-768' : 'ML-KEM-512';
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { kty: jwk.kty, alg: wrongAlg, pub: jwk.pub },
      { name },
      extractable,
      publicUsages),
    { message: 'JWK "alg" Parameter and algorithm name mismatch' });
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, alg: wrongAlg },
      { name },
      extractable,
      privateUsages),
    { message: 'JWK "alg" Parameter and algorithm name mismatch' });

  // Empty usages for private key
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk },
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });

  // Missing pub for private key
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { kty: jwk.kty, alg: jwk.alg, priv: jwk.priv },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK' });
}

(async function() {
  const tests = [];
  for (const vector of testVectors) {
    for (const extractable of [true, false]) {
      tests.push(testImportSpki(vector, extractable));
      tests.push(testImportPkcs8(vector, extractable));
      tests.push(testImportPkcs8SeedOnly(vector, extractable));
      tests.push(testImportPkcs8PrivOnly(vector, extractable));
      tests.push(testImportRawSeed(vector, extractable));
      tests.push(testImportRawPublic(vector, extractable));
      tests.push(testImportJwk(vector, extractable));
    }
  }
  await Promise.all(tests);
})().then(common.mustCall());

(async function() {
  const alg = 'ML-KEM-512';
  const pub = keyData[alg].spki.subarray(-keyData[alg].pub_len);
  await assert.rejects(subtle.importKey('raw', pub, alg, false, []), {
    name: 'NotSupportedError',
    message: 'Unable to import ML-KEM-512 using raw format',
  });
})().then(common.mustCall());
