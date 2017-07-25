'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const exec = require('child_process').exec;
const crypto = require('crypto');

// Test certificates
const certPem = fs.readFileSync(`${common.fixturesDir}/test_cert.pem`, 'ascii');
const keyPem = fs.readFileSync(`${common.fixturesDir}/test_key.pem`, 'ascii');
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
    const errMessage = /^Error:.*data too large for key size$/;

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
        }, errMessage);
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
          const saltLengthCorrect = getEffectiveSaltLength(signSaltLength) ===
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

// Test vectors for RSA_PKCS1_PSS_PADDING provided by the RSA Laboratories:
// https://www.emc.com/emc-plus/rsa-labs/standards-initiatives/pkcs-rsa-cryptography-standard.htm
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

  const vectorfile = path.join(common.fixturesDir, 'pss-vectors.json');
  const examples = JSON.parse(fs.readFileSync(vectorfile, {
    encoding: 'utf8'
  }));

  for (const key in examples) {
    const example = examples[key];
    const publicKey = example.publicKey.join('\n');
    example.tests.forEach((test) => testVerify(publicKey, test));
  }
}

// Test exceptions for invalid `padding` and `saltLength` values
{
  const paddingNotInteger = /^TypeError: padding must be an integer$/;
  const saltLengthNotInteger = /^TypeError: saltLength must be an integer$/;

  [null, undefined, NaN, 'boom', {}, [], true, false]
    .forEach((invalidValue) => {
      assert.throws(() => {
        crypto.createSign('RSA-SHA256')
          .update('Test123')
          .sign({
            key: keyPem,
            padding: invalidValue
          });
      }, paddingNotInteger);

      assert.throws(() => {
        crypto.createSign('RSA-SHA256')
          .update('Test123')
          .sign({
            key: keyPem,
            padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
            saltLength: invalidValue
          });
      }, saltLengthNotInteger);
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

// RSA-PSS Sign test by verifying with 'openssl dgst -verify'
{
  if (!common.opensslCli)
    common.skip('node compiled without OpenSSL CLI.');

  const pubfile = path.join(common.fixturesDir, 'keys/rsa_public_2048.pem');
  const privfile = path.join(common.fixturesDir, 'keys/rsa_private_2048.pem');
  const privkey = fs.readFileSync(privfile);

  const msg = 'Test123';
  const s5 = crypto.createSign('RSA-SHA256')
    .update(msg)
    .sign({
      key: privkey,
      padding: crypto.constants.RSA_PKCS1_PSS_PADDING
    });

  common.refreshTmpDir();

  const sigfile = path.join(common.tmpDir, 's5.sig');
  fs.writeFileSync(sigfile, s5);
  const msgfile = path.join(common.tmpDir, 's5.msg');
  fs.writeFileSync(msgfile, msg);

  const cmd =
    `"${common.opensslCli}" dgst -sha256 -verify "${pubfile}" -signature "${
      sigfile}" -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:-2 "${
      msgfile}"`;

  exec(cmd, common.mustCall((err, stdout, stderr) => {
    assert(stdout.includes('Verified OK'));
  }));
}
