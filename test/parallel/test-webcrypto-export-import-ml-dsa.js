'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const keyData = {
  'ML-DSA-44': {
    jwk: {
      kty: 'AKP',
      alg: 'ML-DSA-44',
      // eslint-disable-next-line @stylistic/js/max-len
      pub: 'gtP6UVGz71mY6VgeI-VIcelf_Dr6FrqOpyU_uU3Vm6NsWHuLd5RhxYdMdi-yZLV2auTSvjL-vaHIdnAatD1bw_ggrvPSIGFdEL4MCM6XTvEovOP_vk9KUBf9w44YZhV5VYezSpJxlT7eYPmCp_yHu2Se6bIqmCBgibi5-ZBhwBM6W4Aojq2a6G-a4M5aH4tBNT7nemoOEiqgteqDWx-xaduFjRShqTzn5OT6TXn1hNIeavnUuTDgxnveA5z_SrIQs_IVMlzgN2a1nnPr0rjO4WLhpNk3vMZ4zqeqQ2iIXMauh0yAM9zI9kR3AIshIg6oWXF7_bq7RXdPobaIWzAhVdhqllyhKnwKhY-mPbYvHz7h1G7lhye1QOwC63gtBxlZ64KAi73nTnxKlaWRFm96gSKBwNZuZVxNLUkpNDmGWZVBjXlnPCCaZ8CYSM2kiuRUJcZaQnS9JJmvf-ZYsZdQvG-crEVGq56MwjUSJtuSPFYdwD5k7Pz1BVJADmI9ushzFVFo8pgfGGkxR9fStg_gVpD4rwnB3-DmiXeRtIcsxkFGzhjUorLauAP1ZQSYqCHZc3OCtLV77bBaGc0I8Xbu-QbF82Cm1xepulXF1MqbroGUqH-sWZn6_7dy8pg1gAXvED-rTZVACr90HnxIcfzRMWpUpbbUk7H1ywA6qHMIXJXKsNjo2gqkae5PVRCskf1NLZ4z8xyDiCC3vkw9_Hd4dUGUhhLwifFOoHuffZg3a5Sxinc4DHW2qyClZ7iN-QfJ00Uz0wZPts1Acwl1dfo2B7UAFpYeL9YlVehQgJvAcfsrffwvSVIWt9XHBmydABVJctrwz-p4s4EJNSK-tXrpgGS6w4uo7_6gDoZyukLYLJOy-YBhMSUrdJg4RhnjezTekV8huCk6qq4oj1JNAzOjKjc-X-CHnhIsyNjxfjcrO2SVntJLMDJWr52eyxcyhKmrmcjJaL39dE21WxGIcAdFAvD9IoAL6fuaSpOwxi2XTDBmXBJwWmdHY2HXGUma2RlfMNe0el-XcPxgAyv1nbypBW1pcK3k9e82mrE0ldNYwPifaZZp9ebQpYJ5Snj6m59twNCGFmiKUn3N67seCEkxzUeLnxcHTGmpQxIt5y53VHY535npd-H0dxRMnISTSrQ2tVtf-vQPU5QW_ldcqiYHRBSNv2Art8NDjYJZQU4qvvRyox5mhH94g3Xoiq5N3lFBtUtBIrMCNFxGHye8sNaOjJcimWBqZdoM1bScDflqnR1W_tOnIlLHJzrNtKHDObdtHbWf0K30Y93g239CZ_pSz7JsKmu2h5CnXRYMKxYY8tzwje222rlvqh8XcDHFBOHGDFT3etQvD7hD1_XaglYwNm_UIl1XRjQuYTNIvekUHFoA7GDOBfE17Txdbir1Ne_QUt_tufBuU5p8CuWbJMWKq50Dt0Oj6GT6eK7rfELsJWrqezcKzXQzm4xTXLKdfx1Lz_INsekVZUu4963VPubqNir0zBGFScznG_zUeGlMIYyLUGxqk7BPXLql_xaaeLblx8HfeMSRi6k4Cw0OFHX6Vy6sPIk5r00D6_ROD6DN8bcExC_3bF8K2qCrg914EY4pZ9BjgVqKVGvp2HWPpgQqvQCq6M5C_FKVhtfsd3f1flO3_xu08R2edvfL-1bSqCy9zEUpEc5vGBfKLY3DnTU5dG5V-xZNwqjWrZKDNQVf5wOyl-PZlWlp3DI36PH5NNlktUy0rMXkT-X-Odjy5xGbFQ',
      priv: '1Z0s934w8QPUceWNuRN5dfrhQb5_4GxQYICWnBFxnOE',
    },
  },
  'ML-DSA-65': {
    jwk: {
      kty: 'AKP',
      alg: 'ML-DSA-65',
      // eslint-disable-next-line @stylistic/js/max-len
      pub: 'gMe-7J_WYur_NsxzG0iYWqh0LHtK8rbArEnvuNwNHYVmB1gLqnMjYMeK7_ScIGigoMddrW0Rjy7iv1srvbIpsx75K0ifzIqtf8z-gjguZCKWLihbt58KuhPmwPmTUmYtW-oztmVGpAMk5oH6dhxAyPJQO8hP5ZIh78e4bcRjYeyA3yCvQtT4tpDRtCDBnXA7mvdDLmitQI0-rvHtA3638EHZmbho1FxAf_BrmdLdUZR3d9-KlEN-7NWRkn52VYGitaG14SXcCorKvaxfo01v0v-BqAeLxBFzwa93NZKVQKrjHx0m8wXKJGkZ7qQPC1HSm40HGps7kxRNjSDL0T7pOwEZorCqL5n1W6GWzMY0NCm5B_t7hQdZenBkSarWJjHKDiRTWpKygDOQLwqKXUjX42Y5kKh4N2H2iXqsCyb5b2p3D8CzXn5OMkTFHARpP4lGr88m22zq_r6f_L-Q_cW0gWhFILxyUQdzwQYtDfVIJVvdrkCEwCyN66nPKQR9b393j1uunJh4iSfV9SVgL3Ok1P5UT7h65cufsaikMNEnsN4XAtUXtFivUkxLFJwSJZMnZpZnVpzgIJWYnmqrHoRhPiHK1JtH9o1XxQcnGpYWDiW6_-WLycpfSbo4pRNkMjr-fGv7xmdXejQELByZMDxcM-QY3B_BRK7gTgmhr-_hjwg6B4x5MUQXKFeON2qmqYqcelkLWi3q-8UuGm5WIRF3u0ODHxqo896wVnq6nTbgycrLfvLUT_AM2qAAxR-w4SfQ6KZxGeGidNe6dFN4TacW8-NbsMzGTuyhzWsZxLIG8kTyveyNMFwqOYMj2mpPTcg52tRu2izczeFQqXiFhRVGATt-KXGMjmfaKavGkf2SUMcTSJqnTHJaZv7QmMNosgXYinkQAz4XyOfwsi8fgXVyfbh_05o2roM6AiEDff3aqD-m0Vp1d-AFHLxYDkRFvuW9BxbQe24sTopODF6hn42Hh_ZQF2EU1cIqFGzecPV1XGeGfP6Df0rO78701LOw1PemGXCsU30Mn1sIK1fmbqK8gHyZwP9mwS6WjxX5ByIMS_iyqQwWvzcFI3d4h5vYCKC9o0uetorwpvcv8SRTR7959qvPF6eYFO8OgvckB1Ydq6KREMpphL7-EXb1Ot-wxm8v9X2ZhUvTtV-LPxx-pq17qmcAM_ifwinktBrbvGifxGJ7Ku4ZFO7RtuCCB7FOWucGXGsHfs7LMwHdF8XRmTeci-XsG983zAKGYDxyulLD81TGrO2o9ongsG4XIAH9y85ebnfCG9QKfOSdPBxtk8nSJXVb4gSOpioeyt0WSnNBxiUyqOUKTLsrzmK4FZPvEQgZhJSbamoE6_MNkuJ40FOmH28Xb56WETJNJjibd9Mj0d6_ozq0ykpAdHLTDO1EHGIVImEqDQlCyp2zQRun4SVH7MDP8zZpSI2g6GlofDT2A5Yft4OPX6VTX3uaePxIOPmZr1MrBiRNrP9NboJQ_uw6X9pWFCu93jts7XQWnVubfKPcDDHFzZhUMy1y7GaZxqxuW0AlI60XrVElqR_RPa6o2LVd1_CAMWPPj3UAIOoZ4tfkDUo_Ss9HStkDV6gswh9whvTCOW-Nt7C4z3dunvtiT4chxq_0d3_xHzqY7xAnbENIdIIVDWhRUQI53_ThwBVGGpKzijNj_IZxFfz0R4pDyp4msYII8jIe4K87bZ0SuKf2MTLL_sI7E0Rqv-kGasONULQGkVoEMqprOS7BY4P7VrADU_riVLBeasJ3gZ0rh9YaN7GkUDsYa0Lp2Ts_NKIOjH3pZOY8MYyuv2bIfgWE4zYVlmmxyACC2K8EwQQe0QArx2DZAKQ6k-MwrwidGRQfe4dJ0-XCYD5947Zx44sTLmYri00awyuCk_o9NG_KLOZU8mpLUmFFQazh2wEsyY0fYgtK95hZfGLsVcci5LPQPl_B8WhgFoKNXxqGAqht7-8YXwGEdUxT68EvAPFXFuQKyOZu4_q64ApPxe1YWTDntePR1_bhOLlK8as0zZB8iJEeZDTVRmTA5CknGDfIt5rSA1_jcSfteXecNk4xTH_iU4CPSpePUES79kQGVG82cD6sQ03N8Y_buP0GDpViZJOB187hh3NhosghozMoqFnFUgcQaYpMxqMYyVEZczaJdQbWnEWdb1ygfmosfRxceHFF9Sb7-SwE8jRXa_5QMTOhu1nohV27Zownoq0Bd9bqBLMNYsuUOa-s6frJkOZVVvr97v6qJMaq9r8Ol-1dXQag1pca-JdNvGizHFp8a1qGY9b7ywgku5MdyL6THA4KCRvgM7Mj1gQ8sQBF9iMtNSn1BVqdjDxaYITe5J77DBqO4piGzTqonhHhxtRFDYmkk9hSe_GkF_ZMqrBtpcQkI1Xl0vHIJc6i8y2rOe_XCYO_cAQYhgBOz6sF6OCSkYkz82ygaM6HEGE16tTT7HPD47uy1iO3kR3yIq43yeqUByfAp8Y2V4t2rKGG7Lfpne1tnXpO53wEeLWiwlQ6--J-DCfoZLTT9PrKhGbh_z7329QRQWfKXTtWD39qA5IjK458oUBSqVpEN5kXXFA08dEIqEAn3YQW3mDTzRliA_3SOycOV0I',
      priv: 'wS0n9kCle6HZA6M7BXlaAyCvUUqeMiEqaiiN4Y0KPM0',
    },
  },
  'ML-DSA-87': {
    jwk: {
      kty: 'AKP',
      alg: 'ML-DSA-87',
      // eslint-disable-next-line @stylistic/js/max-len
      pub: 'R21Wwut7jawo2a2nJrqIk5cm9ay478z7u01197SY4kVDB7FBYqb3XNFrLIlV8N7Adx8MVKu99A0QdmnaoCN133FDlIU8ys9Q92Px9AaUjVWaRoKFLBSwaP-WF3jGckZwBIm5kScwbkQlK60CKlrOpdwobXYKR-2zXH0bDKSTl2kfx6N9uRPm1tYv0ELyRhVGWFG1-OFcN2ir9Tt8Nd4zfPPdpkj938K4Wytoc9V13iqwsYTKMOFBE4RE28Mq_Wx1XYUu1UOCKjYznHUwUQsqTeA62o_IPQapxfcz0wsIqU1ZSdqWvkxFmRXw7kKspvVjgpZEsY6Qj9105-dup8jeOVpWhdwqQ-2tyJtwCorgWlY9UycHXdX0XZlyGYFsI9HZO6oA0LsI8aaZeFEuGwZKoXmkABkyQt6db8kLgua5nowy3Jm5FfZ3aqGukZiS35E98oVSodRywnzDdugQ9KZ_Nt0UTUVzfm-qJFBuw24Ahsf4dRqappZPgA6szFZLa3u7sdFah9HbsYT_PoLLwJ9RzBtS0HdhZr5UHKpcGP96iIxl88tz56NEWNrzMB0XpHDx4wrS8Qdn9Sm3oWnfQFkC61u4vuLSPNDpqQlJfCKac75_-bxxDoB7Pv7KbAJisyBgRgaiilCOWHNF28K_x8dIT_meop4TTbBZzh1tuy_uV0z2sIu-YV9f6ZntDLcLgYCILEqFAWTuXhouFwRes1DwAluIgI9TCfGGPHJ6IFIaenY7-igcJqy2nGulL8gJqiVg8zh1HSOx9puqvYn_GJj7i_kgu7uSDsrpsGSxoKsnVRQ7SGpWmTOvT1lE0-5N6ETAop1jGFzKvW570_NlAcWXoLPnm4qYBKyEs2pAUe8V1bS5xxTL6dkkz1G2w3rAr4iEzpCj_iFHmeapcP4bdsPcHJjSikKZN5kLsgqXWCq1ofTRk2JbkMKgsjiVrU2Yno_2lWm7qYWlsUf5MG4P6fLkuYE8nIb1HGSa52zWa_s0s537IWaGoJloaH2I1rT8KXNx8-JEtyobkqLvwlgkcVHAtIiMD3owGc_3QD9kvVVFV2d104OU_dVPsP4GRvAyCGbyjdB4XheciXgTRKDJunM7_-sKr-8Bn_5vU0QRlzeJsb4bxczppYYz5zcWodcYtfbIcXbwkjLMMO0LkCL7ri59kGbwYxnmDhVfhUxqLmlTbB9_mFm7JlJ9G6ZTQB_zGxmYYQpm8iSPP4fQO-KsqSky-wqfC75K7Xyghu1jAZp5M2cpxQsclUG64MCUOEYpSODSlrkdnkkjc8O-ll8tgj9JuknAl5rqVvyRHLmIPLaKVXMrMxDdyiJWc2niIWSqrg-XPiFda9S92HNV82qyj_QBrMxAuIGNFI3Nrup7MYlVAVFGQkkAAvfbuxbOZB0SBr9a8k3q_Urb65aXnJxo-CGddeyDKe4jUXAp6CB8X9KIFyzIYBTxf7rp-4WcsnLi7_mpNK0BiTKfgeHMKD7c8cygyFft2AMnHDfVH30R9LbTTf_4HGT_UQhlhw3dUaECGrrqJ87dbNBxDK0_5DNa3NUxNIB4uo3VYRaJToV3yCmIZSPy3_REUXLNqMzodMDWsy6FUIV_mi8D2sh6C8iy0tXuAcIjDJTsiHV5w85_yQpZ-KnYJz5Bxn1CtakKf8szA1WQ0ZSxvd0KN6hr-NG-9YKtaqUddWWor9uyNA26x6oKl12GWTCjNI369OJ5s8x0at4Sf6ie8jXUljDmn4JCzwMhAg5T1cu18OIxiiqhGj7lMVHL0CH9uUVy7a6pAzvcSrx5frclHa49Ztj8Op7DOpjBQrkEh_u6Qx4b8Y1PzcjoeZHUrCBnBqKKxD17lchEntc6Mioc1WRrff-pQ1wuHd9vZMKYqMe9_REHeaEq7spmYT8_n1SK9H2zcraeKOflmlNKkDqRtj_uKJvuYIKvIx-9ZgHuXxpoQLlQpSmQvpfZsX2CN8t87TFwPaMIu3OQsA73awnFIDCfESG7RmmC93Nh0R0OlSOU5AgVwoQWzYnOJS4ondQ9DE5dF49XEG49L6zrl2Ka5BXWHwjN357Jx_X-9wyfwwOpUqzJuYrG_CayF_uKqtF-BibLMV0eYdsOdk5icTFJPbBMY_Hkjsg0YtOgchMx3CuGEgU0fK06iGm-lSzvtAlx4OP14PXxcQJDcvRfvumH-AD8NKjHYMFH8LiUPz8FVj-onWx8ikTuSPpjjAEshDp7C3KP0Ck6lloPe6cPIcgs4RB_YNfLPMErIDmxHfiIDjCDKZR5X-NBw-D1P3DIOqi9Ma1AeaMWK-AErHgwz6tz5OLySoC7DbWevXF2bUZhvQuts851HXXDtQ1Is2hiKTqSNFz1s0PRnQRuw-L_CxuNn7R16Q9fakfNCzeE8ucgtYKe8eiLPMNWTLaOr6bgrooTLxFBXWJ8Cy0HC4GQPXSi7p1IJLjk6EST1eBUYym_fq01K8-mD0R25gOVoHgKD5KQs6by0DYGRqZiyd5OwuwF97T3C-oDOoUtHsKvWzsD0b0kyMAwXfUs_kXhcA33y2hV9Hw0CX7aaNkny_9inkgze81BpwO0e_wn3iIqGvJXhRKJ8hSMWzfg7pQnZH6FmgNUXMlon3OWwOXx4UurJBdm7FfsMW49ScV3K050a-DCficbTScYho7cWTUFbAGrytJSwYSSbCdInimycblYmtDKQigwP35HQFOBvbarD6MBP9CHcbvdRaorffBEICEiMry63wxKjkfBv3HYnyGvPp0sAEFujf3kr9pRez8hGNicbw-AC_CusUX-1hZcWfNZiaI-GICBsI894oMVSAs0WHbrQWdqZq8S0SnM1USTFuatTSyrg0NjnPYmPl__-xvYsidSW3xk5jF9Rb4UG9et8wSjqv6ltd192ibcjlYRoUoaMt1T49A83fuHLCZuUYhgV9iB-H5Ysak8b7xJWJpK-ZxVqL8VdEIDLdG0a9Wjedu5NtZ_EWpYj_b2PsJ8lxhGdb58kkunYU7vXShuqfyXhHMRmBuxlHoN3_6tjqmzPtJ91oWU6zi-Ep_n3gohl4SzJbLMl00rJEadlxL7TNiLpAZXAF2ybPC1WrLx-4dRXIQeWnsoZvgrzKvmZWKoVFpWbTWFwUOgcE93z6UhPezw-S2h5fmWsHfGFqMt_Z45DR-NTQqGQAWuO3XaHCaWqpxneIqo8Z5rTrc1w4KH8dsva8AJWrrw7QHmfU6foLllGv04lR73Kf4fVIvUmDhu0FJ1uEulu24cA0AH7-uZcWxIDDQSwDeU79GlxbIYAW8hUOA7Z1iQ6xF4akOIKMqOxRuVRHi8GvmF2r2avSPaltYoHQR5G-EYNIR5bRcE1BNaxN0hhVMyX5UbKCXOkiT4HDU4ckJ_pWjZDT7HhlckpmwPrJXnf-0NT1DqzR-bMeZQQhWljfSsQ3cwThRtiBSqVYOPBIAeYVfpBBLQFvAMgANE',
      priv: 'i4_fRfJesALZ5T9fxzvpXvFtAbSsyAGdaMU-DzQsr5g',
    }
  },
};

const testVectors = [
  {
    name: 'ML-DSA-44',
    privateUsages: ['sign'],
    publicUsages: ['verify']
  },
  {
    name: 'ML-DSA-65',
    privateUsages: ['sign'],
    publicUsages: ['verify']
  },
  {
    name: 'ML-DSA-87',
    privateUsages: ['sign'],
    publicUsages: ['verify']
  },
];

async function testImportJwk({ name, publicUsages, privateUsages }, extractable) {

  const jwk = keyData[name].jwk;

  const tests = [
    subtle.importKey(
      'jwk',
      {
        kty: jwk.kty,
        alg: jwk.alg,
        pub: jwk.pub,
      },
      { name },
      extractable, publicUsages),
    subtle.importKey(
      'jwk',
      jwk,
      { name },
      extractable,
      privateUsages),
  ];

  const [
    publicKey,
    privateKey,
  ] = await Promise.all(tests);

  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(publicKey.extractable, extractable);
  assert.strictEqual(privateKey.extractable, extractable);
  assert.deepStrictEqual(publicKey.usages, publicUsages);
  assert.deepStrictEqual(privateKey.usages, privateUsages);
  assert.strictEqual(publicKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm, privateKey.algorithm);
  assert.strictEqual(privateKey.usages, privateKey.usages);
  assert.strictEqual(publicKey.algorithm, publicKey.algorithm);
  assert.strictEqual(publicKey.usages, publicKey.usages);

  if (extractable) {
    // Test the round trip
    const [
      pubJwk,
      pvtJwk,
    ] = await Promise.all([
      subtle.exportKey('jwk', publicKey),
      subtle.exportKey('jwk', privateKey),
    ]);

    assert.deepStrictEqual(pubJwk.key_ops, publicUsages);
    assert.strictEqual(pubJwk.ext, true);
    assert.strictEqual(pubJwk.kty, 'AKP');
    assert.strictEqual(pubJwk.pub, jwk.pub);

    assert.deepStrictEqual(pvtJwk.key_ops, privateUsages);
    assert.strictEqual(pvtJwk.ext, true);
    assert.strictEqual(pvtJwk.kty, 'AKP');
    assert.strictEqual(pvtJwk.pub, jwk.pub);
    assert.strictEqual(pvtJwk.priv, jwk.priv);

    assert.strictEqual(pubJwk.alg, jwk.alg);
    assert.strictEqual(pvtJwk.alg, jwk.alg);
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

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, use: 'enc' },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK "use" Parameter' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, pub: undefined },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, priv: 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' }, // Public vs private mismatch
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid keyData' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, kty: 'OKP' },
      { name },
      extractable,
      privateUsages),
    { message: 'Invalid JWK "kty" Parameter' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk },
      { name },
      extractable,
      publicUsages), // Invalid for a private key
    { message: /Unsupported key usage/ });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, ext: false },
      { name },
      true,
      privateUsages),
    { message: 'JWK "ext" Parameter and extractable mismatch' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, priv: undefined },
      { name },
      extractable,
      privateUsages), // Invalid for a public key
    { message: /Unsupported key usage/ });

  for (const alg of [undefined, name === 'ML-DSA-44' ? 'ML-DSA-87' : 'ML-DSA-44']) {
    await assert.rejects(
      subtle.importKey(
        'jwk',
        { kty: jwk.kty, pub: jwk.pub, alg },
        { name },
        extractable,
        publicUsages),
      { message: 'JWK "alg" Parameter and algorithm name mismatch' });

    await assert.rejects(
      subtle.importKey(
        'jwk',
        { ...jwk, alg },
        { name },
        extractable,
        privateUsages),
      { message: 'JWK "alg" Parameter and algorithm name mismatch' });
  }

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk },
      { name },
      extractable,
      [/* empty usages */]),
    { name: 'SyntaxError', message: 'Usages cannot be empty when importing a private key.' });

  await assert.rejects(
    subtle.importKey(
      'jwk',
      { kty: jwk.kty, /* missing pub */ alg: jwk.alg },
      { name },
      extractable,
      publicUsages),
    { name: 'DataError', message: 'Invalid keyData' });
}

async function testImportRawPublic({ name, publicUsages }, extractable) {
  const jwk = keyData[name].jwk;
  const pub = Buffer.from(jwk.pub, 'base64url');

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
      { name: name === 'ML-DSA-44' ? 'ML-DSA-65' : 'ML-DSA-44' },
      extractable, publicUsages),
    { message: 'Invalid keyData' });
}

async function testImportRawSeed({ name, privateUsages }, extractable) {
  const jwk = keyData[name].jwk;
  const seed = Buffer.from(jwk.priv, 'base64url');

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

(async function() {
  const tests = [];
  for (const vector of testVectors) {
    for (const extractable of [true, false]) {
      tests.push(testImportJwk(vector, extractable));
      tests.push(testImportRawSeed(vector, extractable));
      tests.push(testImportRawPublic(vector, extractable));
    }
  }
  await Promise.all(tests);
})().then(common.mustCall());

(async function() {
  const alg = 'ML-DSA-44';
  const pub = Buffer.from(keyData[alg].jwk.pub, 'base64url');
  await assert.rejects(subtle.importKey('raw', pub, alg, false, []), {
    name: 'NotSupportedError',
    message: 'Unable to import ML-DSA-44 using raw format',
  });
})().then(common.mustCall());
