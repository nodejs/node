'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

// Test certificates
const certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
const keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');
const modSize = 1024;

// Test signing and verifying
{
  const s1 = crypto.createSign('RSA-SHA1')
                   .update('Test123')
                   .sign(keyPem, 'base64');
  let s1stream = crypto.createSign('RSA-SHA1');
  s1stream.end('Test123');
  s1stream = s1stream.sign(keyPem, 'base64');
  assert.strictEqual(s1, s1stream, 'Stream produces same output');

  const verified = crypto.createVerify('RSA-SHA1')
                         .update('Test')
                         .update('123')
                         .verify(certPem, s1, 'base64');
  assert.strictEqual(verified, true, 'sign and verify (base 64)');
}

{
  const s2 = crypto.createSign('RSA-SHA256')
                   .update('Test123')
                   .sign(keyPem, 'latin1');
  let s2stream = crypto.createSign('RSA-SHA256');
  s2stream.end('Test123');
  s2stream = s2stream.sign(keyPem, 'latin1');
  assert.strictEqual(s2, s2stream, 'Stream produces same output');

  let verified = crypto.createVerify('RSA-SHA256')
                       .update('Test')
                       .update('123')
                       .verify(certPem, s2, 'latin1');
  assert.strictEqual(verified, true, 'sign and verify (latin1)');

  const verStream = crypto.createVerify('RSA-SHA256');
  verStream.write('Tes');
  verStream.write('t12');
  verStream.end('3');
  verified = verStream.verify(certPem, s2, 'latin1');
  assert.strictEqual(verified, true, 'sign and verify (stream)');
}

{
  const s3 = crypto.createSign('RSA-SHA1')
                   .update('Test123')
                   .sign(keyPem, 'buffer');
  let verified = crypto.createVerify('RSA-SHA1')
                       .update('Test')
                       .update('123')
                       .verify(certPem, s3);
  assert.strictEqual(verified, true, 'sign and verify (buffer)');

  const verStream = crypto.createVerify('RSA-SHA1');
  verStream.write('Tes');
  verStream.write('t12');
  verStream.end('3');
  verified = verStream.verify(certPem, s3);
  assert.strictEqual(verified, true, 'sign and verify (stream)');
}

// Special tests for RSA_PKCS1_PSS_PADDING
{
  function testPSS(algo, hLen) {
    // Maximum permissible salt length
    const max = modSize / 8 - hLen - 2;

    function getEffectiveSaltLength(saltLength) {
      switch (saltLength) {
        case crypto.constants.RSA_PSS_SALTLEN_DIGEST:
          return hLen;
        case crypto.constants.RSA_PSS_SALTLEN_MAX_SIGN:
          return max;
        default:
          return saltLength;
      }
    }

    const signSaltLengths = [
      crypto.constants.RSA_PSS_SALTLEN_DIGEST,
      getEffectiveSaltLength(crypto.constants.RSA_PSS_SALTLEN_DIGEST),
      crypto.constants.RSA_PSS_SALTLEN_MAX_SIGN,
      getEffectiveSaltLength(crypto.constants.RSA_PSS_SALTLEN_MAX_SIGN),
      0, 16, 32, 64, 128
    ];

    const verifySaltLengths = [
      crypto.constants.RSA_PSS_SALTLEN_DIGEST,
      getEffectiveSaltLength(crypto.constants.RSA_PSS_SALTLEN_DIGEST),
      getEffectiveSaltLength(crypto.constants.RSA_PSS_SALTLEN_MAX_SIGN),
      0, 16, 32, 64, 128
    ];

    signSaltLengths.forEach((signSaltLength) => {
      if (signSaltLength > max) {
        // If the salt length is too big, an Error should be thrown
        assert.throws(() => {
          crypto.createSign(algo)
            .update('Test123')
            .sign({
              key: keyPem,
              padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
              saltLength: signSaltLength
            });
        }, /^Error:.*data too large for key size$/);
      } else {
        // Otherwise, a valid signature should be generated
        const s4 = crypto.createSign(algo)
                         .update('Test123')
                         .sign({
                           key: keyPem,
                           padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                           saltLength: signSaltLength
                         });

        let verified;
        verifySaltLengths.forEach((verifySaltLength) => {
          // Verification should succeed if and only if the salt length is
          // correct
          verified = crypto.createVerify(algo)
                           .update('Test123')
                           .verify({
                             key: certPem,
                             padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                             saltLength: verifySaltLength
                           }, s4);
          const saltLengthCorrect = getEffectiveSaltLength(signSaltLength) ==
                                    getEffectiveSaltLength(verifySaltLength);
          assert.strictEqual(verified, saltLengthCorrect, 'verify (PSS)');
        });

        // Verification using RSA_PSS_SALTLEN_AUTO should always work
        verified = crypto.createVerify(algo)
                         .update('Test123')
                         .verify({
                           key: certPem,
                           padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                           saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO
                         }, s4);
        assert.strictEqual(verified, true, 'verify (PSS with SALTLEN_AUTO)');

        // Verifying an incorrect message should never work
        verified = crypto.createVerify(algo)
                         .update('Test1234')
                         .verify({
                           key: certPem,
                           padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                           saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO
                         }, s4);
        assert.strictEqual(verified, false, 'verify (PSS, incorrect)');
      }
    });
  }

  testPSS('RSA-SHA1', 20);
  testPSS('RSA-SHA256', 32);
}

// Test vectors for RSA_PKCS1_PSS_PADDING provided by the RSA Laboratories
{
  // We only test verification as we cannot specify explicit salts when signing
  function testVerify(cert, vector) {
    const verified = crypto.createVerify('RSA-SHA1')
                          .update(Buffer.from(vector.message, 'hex'))
                          .verify({
                            key: cert,
                            padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
                            saltLength: vector.salt.length / 2
                          }, vector.signature, 'hex');
    assert.strictEqual(verified, true, 'verify (PSS)');
  }

  // Example 1: A 1024-bit RSA Key Pair
  const ex01Cert =
    '-----BEGIN PUBLIC KEY-----\n' +
    'MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQClbkoOcBAXWJpRh9x+qEHRVvLs\n' +
    'DjatUqRN/rHmH3rZkdjFEFb/7bFitMDyg6EqiKOU3/Umq3KRy7MHzqv84LHf1c2V\n' +
    'CAltWyuLbfXWce9jd8CSHLI8Jwpw4lmOb/idGfEFrMLT8Ms18pKA4Thrb2TE7yLh\n' +
    '4fINDOjP+yJJvZohNwIDAQAB\n' +
    '-----END PUBLIC KEY-----';
  [
    {
      message: 'cdc87da223d786df3b45e0bbbc721326d1ee2af806cc315475cc6f0d9c66' +
               'e1b62371d45ce2392e1ac92844c310102f156a0d8d52c1f4c40ba3aa6509' +
               '5786cb769757a6563ba958fed0bcc984e8b517a3d5f515b23b8a41e74aa8' +
               '67693f90dfb061a6e86dfaaee64472c00e5f20945729cbebe77f06ce78e0' +
               '8f4098fba41f9d6193c0317e8b60d4b6084acb42d29e3808a3bc372d85e3' +
               '31170fcbf7cc72d0b71c296648b3a4d10f416295d0807aa625cab2744fd9' +
               'ea8fd223c42537029828bd16be02546f130fd2e33b936d2676e08aed1b73' +
               '318b750a0167d0',
      salt: 'dee959c7e06411361420ff80185ed57f3e6776af',
      signature: '9074308fb598e9701b2294388e52f971faac2b60a5145af185df5287b5' +
                 'ed2887e57ce7fd44dc8634e407c8e0e4360bc226f3ec227f9d9e54638e' +
                 '8d31f5051215df6ebb9c2f9579aa77598a38f914b5b9c1bd83c4e2f9f3' +
                 '82a0d0aa3542ffee65984a601bc69eb28deb27dca12c82c2d4c3f66cd5' +
                 '00f1ff2b994d8a4e30cbb33c'
    },
    {
      message: '851384cdfe819c22ed6c4ccb30daeb5cf059bc8e1166b7e3530c4c233e2b' +
               '5f8f71a1cca582d43ecc72b1bca16dfc7013226b9e',
      salt: 'ef2869fa40c346cb183dab3d7bffc98fd56df42d',
      signature: '3ef7f46e831bf92b32274142a585ffcefbdca7b32ae90d10fb0f0c7299' +
                 '84f04ef29a9df0780775ce43739b97838390db0a5505e63de927028d9d' +
                 '29b219ca2c4517832558a55d694a6d25b9dab66003c4cccd907802193b' +
                 'e5170d26147d37b93590241be51c25055f47ef62752cfbe21418fafe98' +
                 'c22c4d4d47724fdb5669e843'
    },
    {
      message: 'a4b159941761c40c6a82f2b80d1b94f5aa2654fd17e12d588864679b54cd' +
               '04ef8bd03012be8dc37f4b83af7963faff0dfa225477437c48017ff2be81' +
               '91cf3955fc07356eab3f322f7f620e21d254e5db4324279fe067e0910e2e' +
               '81ca2cab31c745e67a54058eb50d993cdb9ed0b4d029c06d21a94ca661c3' +
               'ce27fae1d6cb20f4564d66ce4767583d0e5f060215b59017be85ea848939' +
               '127bd8c9c4d47b51056c031cf336f17c9980f3b8f5b9b6878e8b797aa43b' +
               '882684333e17893fe9caa6aa299f7ed1a18ee2c54864b7b2b99b72618fb0' +
               '2574d139ef50f019c9eef416971338e7d470',
      salt: '710b9c4747d800d4de87f12afdce6df18107cc77',
      signature: '666026fba71bd3e7cf13157cc2c51a8e4aa684af9778f91849f34335d1' +
                 '41c00154c4197621f9624a675b5abc22ee7d5baaffaae1c9baca2cc373' +
                 'b3f33e78e6143c395a91aa7faca664eb733afd14d8827259d99a7550fa' +
                 'ca501ef2b04e33c23aa51f4b9e8282efdb728cc0ab09405a91607c6369' +
                 '961bc8270d2d4f39fce612b1'
    },
    {
      message: 'bc656747fa9eafb3f0',
      salt: '056f00985de14d8ef5cea9e82f8c27bef720335e',
      signature: '4609793b23e9d09362dc21bb47da0b4f3a7622649a47d464019b9aeafe' +
                 '53359c178c91cd58ba6bcb78be0346a7bc637f4b873d4bab38ee661f19' +
                 '9634c547a1ad8442e03da015b136e543f7ab07c0c13e4225b8de8cce25' +
                 'd4f6eb8400f81f7e1833b7ee6e334d370964ca79fdb872b4d75223b5ee' +
                 'b08101591fb532d155a6de87'
    },
    {
      message: 'b45581547e5427770c768e8b82b75564e0ea4e9c32594d6bff706544de0a' +
               '8776c7a80b4576550eee1b2acabc7e8b7d3ef7bb5b03e462c11047eadd00' +
               '629ae575480ac1470fe046f13a2bf5af17921dc4b0aa8b02bee633491165' +
               '1d7f8525d10f32b51d33be520d3ddf5a709955a3dfe78283b9e0ab54046d' +
               '150c177f037fdccc5be4ea5f68b5e5a38c9d7edcccc4975f455a6909b4',
      salt: '80e70ff86a08de3ec60972b39b4fbfdcea67ae8e',
      signature: '1d2aad221ca4d31ddf13509239019398e3d14b32dc34dc5af4aeaea3c0' +
                 '95af73479cf0a45e5629635a53a018377615b16cb9b13b3e09d671eb71' +
                 'e387b8545c5960da5a64776e768e82b2c93583bf104c3fdb23512b7b4e' +
                 '89f633dd0063a530db4524b01c3f384c09310e315a79dcd3d684022a7f' +
                 '31c865a664e316978b759fad'
    },
    {
      message: '10aae9a0ab0b595d0841207b700d48d75faedde3b775cd6b4cc88ae06e46' +
               '94ec74ba18f8520d4f5ea69cbbe7cc2beba43efdc10215ac4eb32dc302a1' +
               'f53dc6c4352267e7936cfebf7c8d67035784a3909fa859c7b7b59b8e39c5' +
               'c2349f1886b705a30267d402f7486ab4f58cad5d69adb17ab8cd0ce1caf5' +
               '025af4ae24b1fb8794c6070cc09a51e2f9911311e3877d0044c71c57a993' +
               '395008806b723ac38373d395481818528c1e7053739282053529510e935c' +
               'd0fa77b8fa53cc2d474bd4fb3cc5c672d6ffdc90a00f9848712c4bcfe46c' +
               '60573659b11e6457e861f0f604b6138d144f8ce4e2da73',
      salt: 'a8ab69dd801f0074c2a1fc60649836c616d99681',
      signature: '2a34f6125e1f6b0bf971e84fbd41c632be8f2c2ace7de8b6926e31ff93' +
                 'e9af987fbc06e51e9be14f5198f91f3f953bd67da60a9df59764c3dc0f' +
                 'e08e1cbef0b75f868d10ad3fba749fef59fb6dac46a0d6e50436933158' +
                 '6f58e4628f39aa278982543bc0eeb537dc61958019b394fb273f215858' +
                 'a0a01ac4d650b955c67f4c58'
    }
  ].forEach((vector) => testVerify(ex01Cert, vector));

  // Example 10: A 2048-bit RSA Key Pair
  const ex10Cert =
    '-----BEGIN PUBLIC KEY-----\n' +
    'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApd2GesTLAvkLlFfUjBSn\n' +
    'cO+ZHFbDnA7GX9Ea+ok3zqV7m+esc7RcABdhW4LWIuMYdTtgJ8D9FXvhL4CQ/uKn\n' +
    'rc0O73WfiLpJl8ekLVjJqhLLma4AH+UhwTu1QxRFqNWuT15MfpSKwifTYEBx8g5X\n' +
    'fpBfvrFd+vBtHeWuYlPWOmohILMaXaXavJVQYA4g8n03OeJieSX+o8xQnyHf8E5u\n' +
    '6kVJxUDWgJ/5MH7t6R//WHM9g4WiN9bTcFoz45GQCZIHDfet8TV89+NwDONmfeg/\n' +
    'F7jfF3jbOB3OCctK0FilEQAac4GY7ifPVaE7dUU5kGWC7IsXS9WNXR89dnxhNyGu\n' +
    'BQIDAQAB\n' +
    '-----END PUBLIC KEY-----';
  [
    {
      message: '883177e5126b9be2d9a9680327d5370c6f26861f5820c43da67a3ad609',
      salt: '04e215ee6ff934b9da70d7730c8734abfcecde89',
      signature: '82c2b160093b8aa3c0f7522b19f87354066c77847abf2a9fce542d0e84' +
                 'e920c5afb49ffdfdace16560ee94a1369601148ebad7a0e151cf163317' +
                 '91a5727d05f21e74e7eb811440206935d744765a15e79f015cb66c532c' +
                 '87a6a05961c8bfad741a9a6657022894393e7223739796c02a77455d0f' +
                 '555b0ec01ddf259b6207fd0fd57614cef1a5573baaff4ec00069951659' +
                 'b85f24300a25160ca8522dc6e6727e57d019d7e63629b8fe5e89e25cc1' +
                 '5beb3a647577559299280b9b28f79b0409000be25bbd96408ba3b43cc4' +
                 '86184dd1c8e62553fa1af4040f60663de7f5e49c04388e257f1ce89c95' +
                 'dab48a315d9b66b1b7628233876ff2385230d070d07e1666'
    },
    {
      message: 'dd670a01465868adc93f26131957a50c52fb777cdbaa30892c9e12361164' +
               'ec13979d43048118e4445db87bee58dd987b3425d02071d8dbae80708b03' +
               '9dbb64dbd1de5657d9fed0c118a54143742e0ff3c87f74e45857647af3f7' +
               '9eb0a14c9d75ea9a1a04b7cf478a897a708fd988f48e801edb0b7039df8c' +
               '23bb3c56f4e821ac',
      salt: '8b2bdd4b40faf545c778ddf9bc1a49cb57f9b71b',
      signature: '14ae35d9dd06ba92f7f3b897978aed7cd4bf5ff0b585a40bd46ce1b42c' +
                 'd2703053bb9044d64e813d8f96db2dd7007d10118f6f8f8496097ad75e' +
                 '1ff692341b2892ad55a633a1c55e7f0a0ad59a0e203a5b8278aec54dd8' +
                 '622e2831d87174f8caff43ee6c46445345d84a59659bfb92ecd4c81866' +
                 '8695f34706f66828a89959637f2bf3e3251c24bdba4d4b7649da002221' +
                 '8b119c84e79a6527ec5b8a5f861c159952e23ec05e1e717346faefe8b1' +
                 '686825bd2b262fb2531066c0de09acde2e4231690728b5d85e115a2f6b' +
                 '92b79c25abc9bd9399ff8bcf825a52ea1f56ea76dd26f43baafa18bfa9' +
                 '2a504cbd35699e26d1dcc5a2887385f3c63232f06f3244c3'
    },
    {
      message: '48b2b6a57a63c84cea859d65c668284b08d96bdcaabe252db0e4a96cb1ba' +
               'c6019341db6fbefb8d106b0e90eda6bcc6c6262f37e7ea9c7e5d226bd7df' +
               '85ec5e71efff2f54c5db577ff729ff91b842491de2741d0c631607df586b' +
               '905b23b91af13da12304bf83eca8a73e871ff9db',
      salt: '4e96fc1b398f92b44671010c0dc3efd6e20c2d73',
      signature: '6e3e4d7b6b15d2fb46013b8900aa5bbb3939cf2c095717987042026ee6' +
                 '2c74c54cffd5d7d57efbbf950a0f5c574fa09d3fc1c9f513b05b4ff50d' +
                 'd8df7edfa20102854c35e592180119a70ce5b085182aa02d9ea2aa90d1' +
                 'df03f2daae885ba2f5d05afdac97476f06b93b5bc94a1a80aa9116c4d6' +
                 '15f333b098892b25fface266f5db5a5a3bcc10a824ed55aad35b727834' +
                 'fb8c07da28fcf416a5d9b2224f1f8b442b36f91e456fdea2d7cfe33672' +
                 '68de0307a4c74e924159ed33393d5e0655531c77327b89821bdedf8801' +
                 '61c78cd4196b5419f7acc3f13e5ebf161b6e7c6724716ca33b85c2e256' +
                 '40192ac2859651d50bde7eb976e51cec828b98b6563b86bb'
    },
    {
      message: '0b8777c7f839baf0a64bbbdbc5ce79755c57a205b845c174e2d2e90546a0' +
               '89c4e6ec8adffa23a7ea97bae6b65d782b82db5d2b5a56d22a29a05e7c44' +
               '33e2b82a621abba90add05ce393fc48a840542451a',
      salt: 'c7cd698d84b65128d8835e3a8b1eb0e01cb541ec',
      signature: '34047ff96c4dc0dc90b2d4ff59a1a361a4754b255d2ee0af7d8bf87c9b' +
                 'c9e7ddeede33934c63ca1c0e3d262cb145ef932a1f2c0a997aa6a34f8e' +
                 'aee7477d82ccf09095a6b8acad38d4eec9fb7eab7ad02da1d11d8e54c1' +
                 '825e55bf58c2a23234b902be124f9e9038a8f68fa45dab72f66e0945bf' +
                 '1d8bacc9044c6f07098c9fcec58a3aab100c805178155f030a124c450e' +
                 '5acbda47d0e4f10b80a23f803e774d023b0015c20b9f9bbe7c91296338' +
                 'd5ecb471cafb032007b67a60be5f69504a9f01abb3cb467b260e2bce86' +
                 '0be8d95bf92c0c8e1496ed1e528593a4abb6df462dde8a0968dffe4683' +
                 '116857a232f5ebf6c85be238745ad0f38f767a5fdbf486fb'
    },
    {
      message: 'f1036e008e71e964dadc9219ed30e17f06b4b68a955c16b312b1eddf028b' +
               '74976bed6b3f6a63d4e77859243c9cccdc98016523abb02483b35591c33a' +
               'ad81213bb7c7bb1a470aabc10d44256c4d4559d916',
      salt: 'efa8bff96212b2f4a3f371a10d574152655f5dfb',
      signature: '7e0935ea18f4d6c1d17ce82eb2b3836c55b384589ce19dfe743363ac99' +
                 '48d1f346b7bfddfe92efd78adb21faefc89ade42b10f374003fe122e67' +
                 '429a1cb8cbd1f8d9014564c44d120116f4990f1a6e38774c194bd1b821' +
                 '3286b077b0499d2e7b3f434ab12289c556684deed78131934bb3dd6537' +
                 '236f7c6f3dcb09d476be07721e37e1ceed9b2f7b406887bd53157305e1' +
                 'c8b4f84d733bc1e186fe06cc59b6edb8f4bd7ffefdf4f7ba9cfb9d5706' +
                 '89b5a1a4109a746a690893db3799255a0cb9215d2d1cd490590e952e8c' +
                 '8786aa0011265252470c041dfbc3eec7c3cbf71c24869d115c0cb4a956' +
                 'f56d530b80ab589acfefc690751ddf36e8d383f83cedd2cc'
    },
    {
      message: '25f10895a87716c137450bb9519dfaa1f207faa942ea88abf71e9c179800' +
               '85b555aebab76264ae2a3ab93c2d12981191ddac6fb5949eb36aee3c5da9' +
               '40f00752c916d94608fa7d97ba6a2915b688f20323d4e9d96801d89a72ab' +
               '5892dc2117c07434fcf972e058cf8c41ca4b4ff554f7d5068ad3155fced0' +
               'f3125bc04f9193378a8f5c4c3b8cb4dd6d1cc69d30ecca6eaa51e36a0573' +
               '0e9e342e855baf099defb8afd7',
      salt: 'ad8b1523703646224b660b550885917ca2d1df28',
      signature: '6d3b5b87f67ea657af21f75441977d2180f91b2c5f692de82955696a68' +
                 '6730d9b9778d970758ccb26071c2209ffbd6125be2e96ea81b67cb9b93' +
                 '08239fda17f7b2b64ecda096b6b935640a5a1cb42a9155b1c9ef7a633a' +
                 '02c59f0d6ee59b852c43b35029e73c940ff0410e8f114eed46bbd0fae1' +
                 '65e42be2528a401c3b28fd818ef3232dca9f4d2a0f5166ec59c42396d6' +
                 'c11dbc1215a56fa17169db9575343ef34f9de32a49cdc3174922f229c2' +
                 '3e18e45df9353119ec4319cedce7a17c64088c1f6f52be29634100b391' +
                 '9d38f3d1ed94e6891e66a73b8fb849f5874df59459e298c7bbce2eee78' +
                 '2a195aa66fe2d0732b25e595f57d3e061b1fc3e4063bf98f'
    }
  ].forEach((vector) => testVerify(ex10Cert, vector));
}

// Test exceptions for invalid `padding` and `saltLength` values
{
  [null, undefined, NaN, 'boom', {}, [], true, false]
    .forEach((invalidValue) => {
      assert.throws(() => {
        crypto.createSign('RSA-SHA256')
          .update('Test123')
          .sign({
            key: keyPem,
            padding: invalidValue
          });
      }, /^TypeError: padding must be an integer$/);

      assert.throws(() => {
        crypto.createSign('RSA-SHA256')
          .update('Test123')
          .sign({
            key: keyPem,
            padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
            saltLength: invalidValue
          });
      }, /^TypeError: saltLength must be an integer$/);
    });

  assert.throws(() => {
    crypto.createSign('RSA-SHA1')
      .update('Test123')
      .sign({
        key: keyPem,
        padding: crypto.constants.RSA_PKCS1_OAEP_PADDING
      });
  }, /^Error:.*illegal or unsupported padding mode$/);
}

// Test throws exception when key options is null
{
  assert.throws(() => {
    crypto.createSign('RSA-SHA1').update('Test123').sign(null, 'base64');
  }, /^Error: No key provided to sign$/);
}
