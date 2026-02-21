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
  'ML-DSA-44': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-dsa-44', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-dsa-44', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-dsa-44', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-dsa-44', 'public'), 'ascii')),
    jwk: {
      kty: 'AKP',
      alg: 'ML-DSA-44',
      pub: 'fYmD1Rx_jkoW9KG7Bs_5zyYEiWEZs15tYBxNdKq9NircZnvZBwwwaGbj0UsxJNc4Dyfp2IFAZZPO3rFCSUdpXHPrGRHwIVMzwiwfu2V7V02xoheW4mrkPThA3JRJSmNdsx6YGu37MaeJkIk6AlUexo46JfGrkRXZp_IyZxiL_L2dPrfwx-32j7WFI5sBadp7cDWfNkJjdQwW4puTe5Rw7h16GHb-DMOAKpfeMHujh7IYHuLCU6lVi90j1m8Ru0dxdmeQ1eY1vDnO7fNQKfzOLhpUNnj7BBZ24GTqFc-SN5HDCSCsSGKScTYYBwiSVTdSGG1GNqIiN2FgE4z1Jj6JFVB_OIUnl4sKbb3m8kB0BwtUPbkC0FVokGRUEGt6ba1Pc_IMpB5Gs3g9PFREI_C9o1yVW3NS2PzH_Vk4Tpf0N1K1kzIK_3IqekLfyqXmVDNsOovsS7Sw9TdmdWUNGRmhXFKRkex5VjpMIx7OwBGsYJCc4FhauWdrVtbkvHGggSpsla73ZcA4Vzh7aq47LMv0KS2YLp-DMn7SEohPHGg74118eLLn88yptxwtwt1dBFj8BKUfPrytuN1EIRQy34hwbkBLN9wDqhgn3Z3fvksRvmgN_4ZQ8YjeD-H3OFh5WJ_Rd66wHSl-YFat-_JF4UPcdlkNUbxPvDi5VL909Pe3VlwEZhT5otdtXQX4U3dUfqWKEh2kN0Q2lo8wbf3OMmBOFTfyX0eYa_5088ZnJvvliefn-TCDyc6WlcZrNqwBOF8N8-IN3b_8RPq-RuV8-mK-M83Hi4ElQB7Z44eZMmfUwFrozEG4Wq2K6MwQ_edG4dWeUVMCloTpGDFOtlLQlDoAN4m_sS2Lbwm_3ra29noUcK8_j10yy-hENE2Yluh1pIL-GoWZj3uYO-rEKVbszaagdE0DJ_uQcHUdNnBHKn64-cQ6xihXzxaeHx9OxkWWMKbzLtKpuYDK_X7EVvm8YTjl_oTsr2SWT2usjNJko32DhRV-OXLKKHo5FJpCy2bGFLXGG26CglUvgZQ2dyXiWeGVNKffOv1cQ5R_RlU2MpLiZ1bigy9hh4lu_XAHLfjQfhf71jeMuF4nEBWV-YOAjDTaDB2hcGqv_XcGXcmLWHqOWgc5Mb6lkb2zYs_oyOskmyFx6C0P7UrV8kCiN4zbuTqZNdNjlWL_QJUmU3vk6CpNa0XN1M3sLjZpOEsaqgRVPLcIDH-juVhyWiymuxe-8yNCOFSKxhscew08EQ9DEckP_iIA8qU2gcreHtvAS5VA5Emz1K2ypYe6oS3ogP-CX4nOAEfvjsb1HHJoclgiwjL1BtCLFgOE-0vn1M-nVOE6WbHGHoNKMJMHP2a3HQC7DmDfSOw5P6Cj5X7QVqhCY6tAGZWEPu3hUssp7K5UJePEdBn_LrErt4ucyXW6y1PAA2Fn8EuHaRyf2ggibDGnzq8E15m_R4LMvZAuGR0bN9jBTlm_x4ZQMqFwKkIdllkN1QTErazOyNsgU6fhA_20h5EIYT6-LqXr_Otj3Kp8MkJB9c3XNGoo5sbHTQCt0VNOHoxCFP_swiAJLtm743eOsI1M6naWLIqPagSCioosAvJYowypJQGvM-N3hBu8KUr0f911KRN7WqTAXTOHZ_vvTqcWKet0dFdh1EHuP3TrU8hSMciaphGvuK93T3gaWuJ6lcCkQndWvEo9S6FQB7eLU_ALKOQ3ROybUUkXgfyTkWDPxbHdeJCgMRv6Ig1PShPyxYb4ig',
      priv: '273AhMPiZWLlSQCY41yi1fMj6xavGH0btB23zMhI1uY',
    },
  },
  'ML-DSA-65': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-dsa-65', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-dsa-65', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-dsa-65', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-dsa-65', 'public'), 'ascii')),
    jwk: {
      kty: 'AKP',
      alg: 'ML-DSA-65',
      pub: 'hxPP5LvG83t2fJyfA1TUssJK_ydrzryrCHGZuKFxmnl5Y3sxHRCPW_JpHEoiIgR6kgELnwibZnueax1zFerTOTA7o0NwXHFiaEB-8AmqJI93DkvtbUOSTCixa3admQBKW_PtgMCVtaEVuuvCuOEFhOyuZkyfvnpBwUKOkz3t-O1wpgrSmf-rdPXOEv8YcsSn-xfLYPSLzPCnt7gnIX_fwtkgnXjref-QqjFKlKZE2e7MkmHeViJ4iGy78r3UzVhBHsmFGC0ZNc8-iT3muH5Sn0SXmNq-F2EoerWLIAsPxL2KE6UrqPAwTbHn1B5sAGWvhsVVLlFPI1s1JLVLBNRJ5vhif525xNIpMAMuAZrteD827pve3zQo9_GHjWgykj9VzM9PEcVmVqxZ5u41kUXsM4PWZF29Oh2sYsmJ2LdiJ9RcA91vRLG2DqEYm-V5JwIz8uxL17DUsEC7zYthvtqGASq05CbfPTBev33rQUv4H0Etz99U89WooTk0FisHDz1uEUilU_VY1tN5byIDitXNf0jnz3SIHDUUZARn7ll0YwO0jtksT68sQW3Liy6Exhlp1td0so2qZUrbVZasjyCOVuibwbwvrdpP3QRsoG5UqkAqk8Rm2iCpdQSg87pswOscgA8AC8TczGHNfXc9PqzAmbsEPKvmZuE60HLGzqpRqFULf3nyYUQUqbdmKJsKQ29LXeDVbyy3-fkTUDuYqNC2tBY7PkzHJSA9Z4hDC_BHEFxcelibScSNyf7y4lDVWnuJMXpQ0WRh3UkUPa007IerhixwxvBvFXQR-ytYinixvjirlcEF1wQI1DzE8KjOXYYuFPS4Yl8HeZHQ-64Q0RuxlKIRP3YvjZWh4IDVvEVs5ZLzZPbE3Twe0N5a7iCu0BzZWTeHNbcMoViFyJTpec2w3vVHeI4PJB-5HeI8xuh-9y8ytTau8QtMe4thoROoajizDQLrkw3e6ryJJ3R84i0oni4vmZWyLDilwcLqPOkQJCIDMjq7exdmVX5t3DtAW4F6Coz0z3sf7tGlSMVxA7izCoVbG0y2_l1P2h7fWBuEPT7PWlMdPqu9Pj5jqXY6jJ0nkaR_pp7dDhO1HKae5edcBYunHZqVQQjRZ_DvKzbPrDk5t6Xq9fdSkiAeP3B4qn5uU-nx7OaX7DRoVEnbbiEDynIRPSEY-Ts3alPJtBv8zuzaGNyX05Z9MyZ0w-VlC-WxOBdVEsIAp_4uJ3kQ3UsfE9DLJH8WPuDI4t4i2VnNNyFlI0XSUocc_0rWgqp2I1UzSzkVbklwkuFywPI645u4G2XAlfdd_wpjFGC-IUPXgpeSfspPwW15sBP-ITS-gwtvfzQVLpRS0euzN97xo_GMhNPZ4bW-YyZt8z_R8bsQ8ktfoP-5RUV-yzYDt0tA01QJsZdBLf5J_H7qP8l4c8V4hPe_CFL032obbxmAnVPAP69u2SaMBlL8azjk4wGVFQpQp1JqMJCao2W8ZImCVegkPZxhGbx0nkgVfyFx4ihMeDNM288JbGC4CGON8C02Q84rQzhwzZE83Y9rSe1Bb0fUMHMu6ihD5jLdeltuBL4ZdJlKgL24KZK5o5pq4_l9SyzGAjB1KAQnClNOB1SxV89CtILu-65wb17s0z3qw2-NF0B6UVlGQFebjbSyLQv2ARaETh_8cBiPugVMgBIV3K1KBwNyWejyI1ZDCssvIZHJCF2SRW3HmJerTiB23eGHFYKSLdxW7LEzoHIc2xZEc3pwR43gavjeoL0pNc-HNFV_c19wiH7Tnw3IHld_FfTqAIPnqKMNIY7D_D0DmFNTOdnzcipqKxUB0Avc-wr8Fz0gjeRpLH2iDSCJWtvWjoeYvHTktGsblDAM5j9xznwEvZfQvj8fTUnFxl3clkD6e9V1jrDQDkXfOtl-bDIv9PtMwamfJFu-z2ubF-gKytUewPNo10uhwr2TDNdUayCZDR2T3HoRLN8goIw2bFoPJ98LoPcSukEvKABjH0DiHNeqFELNZPx_uCx5N-YFkUZxHWA1QUoGhqQ3REtcT3c-SZf_TDFOPvws6bmwt4lcWpLmubOAJLFt6J8m8HCkVUshRdFzvHQm_0JEvA3JtyXZzvsPUv5njdk0nxTZktvsnqX054RQk5x8U-lBY-bK3uMOoFnHju45LMoHCUgGJi22eUm7nLGZEh84ZAbNlPLXpfavXvPJh21OW5EOAeuQ-yWNHY2xbmAiHNnb-J2VpZc1Vy82sxn8umFtKduuQuIQMOsf4qHqj5MzDY_1NjrM4Wm7XAiLC4MpQ22w9PWNQXSZWvo2fj8WUnfEibpgyRkoD2P25GRQqsRJ3-Ykl5bm_2Vfe6i3oXHOwQwZwKGXfAqXyo4iU1UI7e-qC4sj5U64oB_A_NSBaJJrZoQ2fVeGTnFxA4QMMoWCT0VlwBXK0B3jht8Xal3WcI-i9ctQB1-GrmwwgG2ttePHt1IKy69bSZE3FLkFicaHg6VxypG6ef8rVsmMrfpTATOnF5_iEaLNY9428HHGW0iz4vXwaE-MkYy7NK2KMPFiCB0ec9OjIROwayK4LREv4qknWHnVQRSm25Rr9DcVFXKj16Au7X1hv7TuVH7h25U',
      priv: '1X9VEr_iXMRwBvnSytEmHrtA-DpD6FWAUqMrDNlJVBg',
    },
  },
  'ML-DSA-87': {
    pkcs8_seed_only: toDer(fixtures.readKey(getKeyFileName('ml-dsa-87', 'private_seed_only'), 'ascii')),
    pkcs8: toDer(fixtures.readKey(getKeyFileName('ml-dsa-87', 'private'), 'ascii')),
    pkcs8_priv_only: toDer(fixtures.readKey(getKeyFileName('ml-dsa-87', 'private_priv_only'), 'ascii')),
    spki: toDer(fixtures.readKey(getKeyFileName('ml-dsa-87', 'public'), 'ascii')),
    jwk: {
      kty: 'AKP',
      alg: 'ML-DSA-87',
      pub: 'DZXqaBATRN0GRtigxzkLxp7C9fFYxI7Gl-tdfXqJHbzVCTTvRfRwZcu3YmpsUYXBdX2pVsQ51QlqxMslKBRmfNCanBLcfd57qoEIb0K6GIKZGHxlsr9aXNjEGcKMo0ICon0LYTvTWrl72Oz-2yEA_abPK3_dBUFGAYQ6kOQhAHcT1CMmTTck23PnEd5WUpYfZOA9giFX9dNVrrdFWczj_vDOty81ObNKsxfVWT1nG7c60UCJxb2c2tMrBx3rp7Hfc_aOg54W5KHocJi0Eai0ok4buySTe0UCSCUTkeoCcdiABgOFBiXRYzpm3Lz4uot6hSgFpuh67fE9Zpgtn64vfI-O1mgcPrPPpd3yA92Jrq-dvXuM55w1RmA_hha3U5Sh2vm0tD1U57q945UppFReIv_8NAKBkxQ_vHil7ySm-m7IAM-sTUY86_IqMZqisxoz7Ff7ZR2vIiUm3-L0ow4B8uPsCv2ZlUoVXvMF6XQiOHsgqgP1rfH8DmfmPFudwiXrAW6wEmi10skPmkN92aC3TPG6nmaNryQ7f8J82yVmGxW9U7zMbg21qNkRGBi_1YwEt6D8V2pUWv5U1a4p4-Ma0f4uQG4g0odM-WGomlh7pZWZf3sffiPXk9wBrGisxtCJuaB5vtkheWxpEfWqnhc3QdOWfrsRg6P1h7M95SNVW0U8A38wwrqPOpzEnckCVdCrZz2b2KVln6a4twfINg1-3lEZR4rkEmTaTYlLFlXzbRFWBBPGATxeRxhQ_9N5VhHi7STWPFD5HIJyVqz436bbVvM6Py_oldT_xt_tWlPc0w4Pesy2CgaCPlJCnx6cjEg_sRBUcRkBoHqa7aZj4JFFm9bzEaiJ2MKfkHVT4xdbEimMHsD0HkIQpg5-zoB2Jsqgc6Qi3L57hZi-Q1V0G2lmdZ5WZkQ2m5hxle4hHtAmghgynK2p0qzDWHScxHcdd2sInYqQgMYbnvs04YYKWpIfndCUBs9q_EONN5tn8gfSwHEKlQ-KpplEL5kc-99h2uaZsxRlJOF6_z8EZ-aKaKY8jvoAV0g4kZJH5UKy_MkBiva6r-zXUmo88qJjXQatOOSdfvJUTiZiSfcpBQqF9SSDD9WWsgInaOCKO_fAFf9fuacXDMEj0esUx1YrVEe_77S5uObg-UrK405U1JhKJJvd7o8xQKxenv5BJdsbbyYQDbSSe9BrCqeHEgmfRHTXdSvl_3QOP0Ej-dT8YJJHZ1lrujU7Zg5f5Kg99tU5GdLMbHX2kt4F2a0NX09HikEemvUg4NLPhjOihfVkChr-zdF69nfsnTiaQrMgpIcl9jttN99_8Gju-LU8OWbb92m9RLxAUFP115v22f77YPoILm92IjMZMkxEhGneoclWhnudkyR7YoTBjCnT5b7AC9_05uls637FmVf7Ck8-MF4gLil3dstXi4g24bitYhxxqWwiqF4vsDouSGUnuKCMwx3TLsII_xk77TjpQP4vpLdYM3tn94AVlTMMhnI-OZkVJk-_mIbywCwRHlQb5nzVCc0BWlM1kb9PJys2IfciS8LWEoxeq9moDX5w72yJKoLN3CWpD3VdJAiW79zUaySw-IeW0XaHnlze5fYnOozG8lIeyQ9sMZasMiFovGnR3b7jyMtA38U33v16fouWuBILOu0m_QOpRDI9i3rjRM6hdC48zCtNSzc1_1VPYkWDSFK1oVAjdd8-2rjyqdPeUwnqD26VA9_d3R7x8ThrazdbRC8U1hr9jpqNHuZ4LGYu3Ui8wB-lSt9QMaHz517MY_zBEoNGyvbQWtlM7mvLu12KoMM7nvGrPJnvD-HmxTqsVQolD8_lIV5ao72yiKDpArVr6RuV4PpI0j_Wy4-yDCuwBW0gjnB9GvCwOTeByYXJT6Ul7dgHck4BbF3IyFgvmY--ceWr5mBrbAC9LJP_4Wf5O6ul3hFrhiG6zSV4zzBYLnEwfW6LNLEZjZKgmBYiC5s1xlxYWDdcQ5FGmLQ9uEDkr4VItXQWvIIdeBQPyujxmd965Mig9-Sa7SCyV_3wH8fQnGlvU-jJMGL0zvzB2gcu7hMLMagUBj1AKXj-UxpbX1i95f2TOiZDwMeCCszgvCjQ21XKg07TBXrrOiFcgcADgdo-HJr7O1T3ozOIulq1PDM7QZH6i3wDD0j3b0NCdqKCWqhfLXz2-FszyUHmA_GCzOLVzrLT2DcGWIcQbkvF0yZPgTyqKArKa8qytOerdH6oCJ0bRl96855sMVjuUdyVLX7XW_rVTwsOwV0gVAx8SrzovtDFeHRNl7BQKMsyQ1BjWu25jqKJ598vAi3LCZv0kMdiC24qPdgZU4e2aUkco11EnD6nJgdqsVFxufCl4BD_D9g5Wy42fJt4ZgNPAcbUf341KERyReeBEQj-qlPB3IUTIXcJw68GScebhxb0W_tGKMBC6-ip4QfNW7UTxUxVxmCV7h0yRnBBlkuUR1eYQwWRmEPjKd3dLHvgHtr266NQmE1tnKtJlsdPKb0ztrI9vogsENsgGNFQ2tHoeX8vqxcagGznlPVPfc3DlqjBSeTFQaPWvmCQHVKgxkbvffKzFQyFvXEqt6bGGtkwBoRJ_IIwtSeWQ2nFPBe3rlyKrtSnQFIMibJbbYvPVE03Cld9R61r-GGDSQz4aXekLzePEVwnxpe4mWJGco7ctQyE73PekL1uo2g0bRK-KgaE878OiLRBo5T7c633xEf2hMy9532M2GVdTZuoE0LL-wpAh9GmNdvJZc7g2sINvwZi778v2WHcYEKqXvdmrX-Shyh3QkzgIGZrDzM4UlxxUWaXfZ0Z6PNguk7Jafqf3xuUe9Z8zfAJl5c_VA3k8dn7IRg99hRsh-TGBCzqzgjJq4p4XMWP2QuxFTGSgHRe2GSCFzd5-lPjrj76ZyT3MPUxQe_bV2VE-Oys3MT-VkCCM8jFCANXdrfltG6jSiUZ2uJUWNqdNnxPglmmyrgff_m-5CyIRWYXQsIdZGspqdjzb4F6RbBKfL2PQlM6zUfo9JNmE8YQq815Nxkex8vDOrImnew312fZA6rRjr9_uE4lEbw3U7PFlCKBUPvPnsdgedjKYhiS0xU6iS1NDKOvYhcrkCkiU67EmFD4U0-OCv9Kpbb5bIxTJuv405NxJBElAMVI-ya0ns7D4-xUPn05E7PhtGZT0eHwItjT6omThTsTHwB_bQqYfNrjrObO1l1go2hQ-cUadZYsG5l47CB5RlFhANtaC8tiq4KJi48TmEEApB_0VwOI4EmI7SR0oaqx3HRXZfeGevCx2yC9aCYM4HqcyqP2g_1HwsOYzwq4XDEbK5Yl1dtYABxPoo7t8FBq2sSmfrBJWFv_nvreb_DPwbfoSeCy9knqvOktSQRrPMmo-nNGpandBvjmrjSk3EdeziAP7XNre5I-bn_2voDxkzGFtUM-wzlL379ASRGej8FkNWaOyqGP6Anq5PSJ',
      priv: 'LZSOlEPbU9S5_mSsMULffTyxZu6qKEOQ1nfEi2NCscg',
    }
  },
};
/* eslint-enable @stylistic/js/max-len */

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
        message: /key is not extractable/
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
        message: /key is not extractable/
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
        message: /key is not extractable/
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
        message: /key is not extractable/
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
      tests.push(testImportSpki(vector, extractable));
      tests.push(testImportPkcs8(vector, extractable));
      tests.push(testImportPkcs8SeedOnly(vector, extractable));
      tests.push(testImportPkcs8PrivOnly(vector, extractable));
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
