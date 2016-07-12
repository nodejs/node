'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

//
// Test authenticated encryption modes.
//
// !NEVER USE STATIC IVs IN REAL LIFE!
//

const TEST_CASES = [
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4be13896f64dfa2c2d0f2c76',
    tag: '272b422f62eb545eaa15b5ff84092447', tampered: false },
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4be13896f64dfa2c2d0f2c76', aad: '000000FF',
    tag: 'ba2479f66275665a88cb7b15f43eb005', tampered: false },
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4be13596f64dfa2c2d0fac76',
    tag: '272b422f62eb545eaa15b5ff84092447', tampered: true },
  { algo: 'aes-256-gcm',
    key: '337a54767a7233703637564336316a6d56353472495975313534357834546c59',
    iv: '36306950306836764a6f4561', plain: 'Hello node.js world!',
    ct: '58e62cfe7b1d274111a82267ebb93866e72b6c2a',
    tag: '9bb44f663badabacae9720881fb1ec7a', tampered: false },
  { algo: 'aes-256-gcm',
    key: '337a54767a7233703637564336316a6d56353472495975313534357834546c59',
    iv: '36306950306836764a6f4561', plain: 'Hello node.js world!',
    ct: '58e62cff7b1d274011a82267ebb93866e72b6c2b',
    tag: '9bb44f663badabacae9720881fb1ec7a', tampered: true },
  { algo: 'aes-192-gcm',
    key: '1ed2233fa2223ef5d7df08546049406c7305220bca40d4c9',
    iv: '0e1791e9db3bd21a9122c416', plain: 'Hello node.js world!',
    password: 'very bad password', aad: '63616c76696e',
    ct: 'dda53a4059aa17b88756984995f7bba3c636cc44',
    tag: 'd2a35e5c611e5e3d2258360241c5b045', tampered: false },

  // Following test cases are from
  //   http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/
  //    proposedmodes/gcm/gcm-revised-spec.pdf

  // Test case 1
  { algo: 'aes-128-gcm',
    key: '00000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '',
    plainIsHex: false,
    ct: '',
    tag: '58e2fccefa7e3061367f1d57a4e7455a', tampered: false },

  // Test case 2
  { algo: 'aes-128-gcm',
    key: '00000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '00000000000000000000000000000000',
    plainIsHex: true,
    ct: '0388dace60b6a392f328c2b971b2fe78',
    tag: 'ab6e47d42cec13bdf53a67b21257bddf', tampered: false },

  // Test case 3
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a721' +
           'c3c0c95956809532fcf0e2449a6b525b1' +
           '6aedf5aa0de657ba637b391aafd255',
    plainIsHex: true,
    ct: '42831ec2217774244b7221b784d0d49c' +
        'e3aa212f2c02a4e035c17e2329aca12e2' +
        '1d514b25466931c7d8f6a5aac84aa051b' +
        'a30b396a0aac973d58e091473f5985',
    tag: '4d5c2af327cd64a62cf35abd2ba6fab4', tampered: false },

  // Test case 4
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a721' +
           'c3c0c95956809532fcf0e2449a6b525b16' +
           'aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '42831ec2217774244b7221b784d0d49c' +
        'e3aa212f2c02a4e035c17e2329aca12e2' +
        '1d514b25466931c7d8f6a5aac84aa051b' +
        'a30b396a0aac973d58e091',
    tag: '5bc94fbc3221a5db94fae95ae7121a47', tampered: false },

  // Test case 5, 8 byte IV
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbad',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeef' +
         'abaddad2',
    plainIsHex: true,
    ct: '61353b4c2806934a777ff51fa22a4755' +
        '699b2a714fcdc6f83766e5f97b6c7423' +
        '73806900e49f24b22b097544d4896b42' +
        '4989b5e1ebac0f07c23f4598',
    tag: '3612d2e79e3b0785561be14aaca2fccb', tampered: false },

  // Test case 6, 60 byte IV
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: '9313225DF88406E555909C5AFF5269AA' +
        '6A7A9538534F7DA1E4C303D2A318A728' +
        'C3C0C95156809539FCF0E2429A6B52541' +
        '6AEDBF5A0DE6A57A637B39B',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '8ce24998625615b603a033aca13fb894' +
        'be9112a5c3a211a8ba262a3cca7e2ca7' +
        '01e4a9a4fba43c90ccdcb281d48c7c6f' +
        'd62875d2aca417034c34aee5',
    tag: '619cc5aefffe0bfa462af43c1699d050', tampered: false },

  // Test case 7
  { algo: 'aes-192-gcm',
    key: '000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '',
    plainIsHex: false,
    ct: '',
    tag: 'cd33b28ac773f74ba00ed1f312572435', tampered: false },

  // Test case 8
  { algo: 'aes-192-gcm',
    key: '000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '00000000000000000000000000000000',
    plainIsHex: true,
    ct: '98e7247c07f0fe411c267e4384b0f600',
    tag: '2ff58d80033927ab8ef4d4587514f0fb', tampered: false },

  // Test case 9
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b391aafd255',
    plainIsHex: true,
    ct: '3980ca0b3c00e841eb06fac4872a2757' +
        '859e1ceaa6efd984628593b40ca1e19c' +
        '7d773d00c144c525ac619d18c84a3f47' +
        '18e2448b2fe324d9ccda2710acade256',
    tag: '9924a7c8587336bfb118024db8674a14', tampered: false },

  // Test case 10
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '3980ca0b3c00e841eb06fac4872a2757' +
        '859e1ceaa6efd984628593b40ca1e19c' +
        '7d773d00c144c525ac619d18c84a3f47' +
        '18e2448b2fe324d9ccda2710',
    tag: '2519498e80f1478f37ba55bd6d27618c', tampered: false },

  // Test case 11
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: 'cafebabefacedbad',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '0f10f599ae14a154ed24b36e25324db8' +
        'c566632ef2bbb34f8347280fc4507057' +
        'fddc29df9a471f75c66541d4d4dad1c9' +
        'e93a19a58e8b473fa0f062f7',
    tag: '65dcc57fcf623a24094fcca40d3533f8', tampered: false },

  // Test case 12, 60 byte IV
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: '9313225df88406e555909c5aff5269aa' +
        '6a7a9538534f7da1e4c303d2a318a728' +
        'c3c0c95156809539fcf0e2429a6b5254' +
        '16aedbf5a0de6a57a637b39b',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: 'd27e88681ce3243c4830165a8fdcf9ff' +
        '1de9a1d8e6b447ef6ef7b79828666e45' +
        '81e79012af34ddd9e2f037589b292db3' +
        'e67c036745fa22e7e9b7373b',
    tag: 'dcf566ff291c25bbb8568fc3d376a6d9', tampered: false },

  // Test case 13
  { algo: 'aes-256-gcm',
    key: '0000000000000000000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '',
    plainIsHex: false,
    ct: '',
    tag: '530f8afbc74536b9a963b4f1c4cb738b', tampered: false },

  // Test case 14
  { algo: 'aes-256-gcm',
    key: '0000000000000000000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '00000000000000000000000000000000',
    plainIsHex: true,
    ct: 'cea7403d4d606b6e074ec5d3baf39d18',
    tag: 'd0d1c8a799996bf0265b98b5d48ab919', tampered: false },

  // Test case 15
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b391aafd255',
    plainIsHex: true,
    ct: '522dc1f099567d07f47f37a32a84427d' +
        '643a8cdcbfe5c0c97598a2bd2555d1aa' +
        '8cb08e48590dbb3da7b08b1056828838' +
        'c5f61e6393ba7a0abcc9f662898015ad',
    tag: 'b094dac5d93471bdec1a502270e3cc6c', tampered: false },

  // Test case 16
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '522dc1f099567d07f47f37a32a84427d' +
        '643a8cdcbfe5c0c97598a2bd2555d1aa' +
        '8cb08e48590dbb3da7b08b1056828838' +
        'c5f61e6393ba7a0abcc9f662',
    tag: '76fc6ece0f4e1768cddf8853bb2d551b', tampered: false },

  // Test case 17, 8 byte IV
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbad',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: 'c3762df1ca787d32ae47c13bf19844cb' +
        'af1ae14d0b976afac52ff7d79bba9de0' +
        'feb582d33934a4f0954cc2363bc73f78' +
        '62ac430e64abe499f47c9b1f',
    tag: '3a337dbf46a792c45e454913fe2ea8f2', tampered: false },

  // Test case 18, 60 byte IV
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: '9313225df88406e555909c5aff5269aa' +
        '6a7a9538534f7da1e4c303d2a318a728' +
        'c3c0c95156809539fcf0e2429a6b5254' +
        '16aedbf5a0de6a57a637b39b',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '5a8def2f0c9e53f1f75d7853659e2a20' +
        'eeb2b22aafde6419a058ab4f6f746bf4' +
        '0fc0c3b780f244452da3ebf1c5d82cde' +
        'a2418997200ef82e44ae7e3f',
    tag: 'a44a8266ee1c8eb0c8b5d4cf5ae9f19a', tampered: false },
];

var ciphers = crypto.getCiphers();

for (var i in TEST_CASES) {
  var test = TEST_CASES[i];

  if (ciphers.indexOf(test.algo) == -1) {
    common.skip('unsupported ' + test.algo + ' test');
    continue;
  }

  if (common.hasFipsCrypto && test.iv.length < 24) {
    console.log('1..0 # Skipped: IV len < 12 bytes unsupported in FIPS mode');
    continue;
  }

  {
    const encrypt = crypto.createCipheriv(test.algo,
      Buffer.from(test.key, 'hex'), Buffer.from(test.iv, 'hex'));
    if (test.aad)
      encrypt.setAAD(Buffer.from(test.aad, 'hex'));

    const inputEncoding = test.plainIsHex ? 'hex' : 'ascii';
    let hex = encrypt.update(test.plain, inputEncoding, 'hex');
    hex += encrypt.final('hex');

    const auth_tag = encrypt.getAuthTag();
    // only test basic encryption run if output is marked as tampered.
    if (!test.tampered) {
      assert.strictEqual(hex, test.ct);
      assert.strictEqual(auth_tag.toString('hex'), test.tag);
    }
  }

  {
    const decrypt = crypto.createDecipheriv(test.algo,
      Buffer.from(test.key, 'hex'), Buffer.from(test.iv, 'hex'));
    decrypt.setAuthTag(Buffer.from(test.tag, 'hex'));
    if (test.aad)
      decrypt.setAAD(Buffer.from(test.aad, 'hex'));

    const outputEncoding = test.plainIsHex ? 'hex' : 'ascii';

    let msg = decrypt.update(test.ct, 'hex', outputEncoding);
    if (!test.tampered) {
      msg += decrypt.final(outputEncoding);
      assert.equal(msg, test.plain);
    } else {
      // assert that final throws if input data could not be verified!
      assert.throws(function() { decrypt.final('ascii'); }, / auth/);
    }
  }

  {
    if (!test.password) return;
    if (common.hasFipsCrypto) {
      assert.throws(() => { crypto.createCipher(test.algo, test.password); },
                    /not supported in FIPS mode/);
    } else {
      const encrypt = crypto.createCipher(test.algo, test.password);
      if (test.aad)
        encrypt.setAAD(Buffer.from(test.aad, 'hex'));
      let hex = encrypt.update(test.plain, 'ascii', 'hex');
      hex += encrypt.final('hex');
      const auth_tag = encrypt.getAuthTag();
      // only test basic encryption run if output is marked as tampered.
      if (!test.tampered) {
        assert.strictEqual(hex, test.ct);
        assert.strictEqual(auth_tag.toString('hex'), test.tag);
      }
    }
  }

  {
    if (!test.password) return;
    if (common.hasFipsCrypto) {
      assert.throws(() => { crypto.createDecipher(test.algo, test.password); },
                    /not supported in FIPS mode/);
    } else {
      const decrypt = crypto.createDecipher(test.algo, test.password);
      decrypt.setAuthTag(Buffer.from(test.tag, 'hex'));
      if (test.aad)
        decrypt.setAAD(Buffer.from(test.aad, 'hex'));
      let msg = decrypt.update(test.ct, 'hex', 'ascii');
      if (!test.tampered) {
        msg += decrypt.final('ascii');
        assert.strictEqual(msg, test.plain);
      } else {
        // assert that final throws if input data could not be verified!
        assert.throws(function() { decrypt.final('ascii'); }, / auth/);
      }
    }
  }

  // after normal operation, test some incorrect ways of calling the API:
  // it's most certainly enough to run these tests with one algorithm only.

  if (i > 0) {
    continue;
  }

  {
    // non-authenticating mode:
    const encrypt = crypto.createCipheriv('aes-128-cbc',
      'ipxp9a6i1Mb4USb4', '6fKjEjR3Vl30EUYC');
    encrypt.update('blah', 'ascii');
    encrypt.final();
    assert.throws(() => { encrypt.getAuthTag(); }, / state/);
    assert.throws(() => { encrypt.setAAD(Buffer.from('123', 'ascii')); },
                  / state/);
  }

  {
    // trying to get tag before inputting all data:
    const encrypt = crypto.createCipheriv(test.algo,
      Buffer.from(test.key, 'hex'), Buffer.from(test.iv, 'hex'));
    encrypt.update('blah', 'ascii');
    assert.throws(function() { encrypt.getAuthTag(); }, / state/);
  }

  {
    // trying to set tag on encryption object:
    const encrypt = crypto.createCipheriv(test.algo,
      Buffer.from(test.key, 'hex'), Buffer.from(test.iv, 'hex'));
    assert.throws(() => { encrypt.setAuthTag(Buffer.from(test.tag, 'hex')); },
                  / state/);
  }

  {
    // trying to read tag from decryption object:
    const decrypt = crypto.createDecipheriv(test.algo,
      Buffer.from(test.key, 'hex'), Buffer.from(test.iv, 'hex'));
    assert.throws(function() { decrypt.getAuthTag(); }, / state/);
  }

  {
    // trying to create cipher with incorrect IV length
    assert.throws(function() {
      crypto.createCipheriv(
        test.algo,
        Buffer.from(test.key, 'hex'),
        Buffer.alloc(0)
      );
    }, /Invalid IV length/);
  }
}
