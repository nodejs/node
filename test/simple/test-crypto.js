// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.




var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

crypto.DEFAULT_ENCODING = 'buffer';

var fs = require('fs');
var path = require('path');

// Test Certificates
var caPem = fs.readFileSync(common.fixturesDir + '/test_ca.pem', 'ascii');
var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
var certPfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');
var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');
var rsaPubPem = fs.readFileSync(common.fixturesDir + '/test_rsa_pubkey.pem',
    'ascii');
var rsaKeyPem = fs.readFileSync(common.fixturesDir + '/test_rsa_privkey.pem',
    'ascii');

try {
  var credentials = crypto.createCredentials(
                                             {key: keyPem,
                                               cert: certPem,
                                               ca: caPem});
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

// PFX tests
assert.doesNotThrow(function() {
  crypto.createCredentials({pfx:certPfx, passphrase:'sample'});
});

assert.throws(function() {
  crypto.createCredentials({pfx:certPfx});
}, 'mac verify failure');

assert.throws(function() {
  crypto.createCredentials({pfx:certPfx, passphrase:'test'});
}, 'mac verify failure');

assert.throws(function() {
  crypto.createCredentials({pfx:'sample', passphrase:'test'});
}, 'not enough data');

// Test HMAC
var h1 = crypto.createHmac('sha1', 'Node')
               .update('some data')
               .update('to hmac')
               .digest('hex');
assert.equal(h1, '19fd6e1ba73d9ed2224dd5094a71babe85d9a892', 'test HMAC');

// Test HMAC (Wikipedia Test Cases)
var wikipedia = [
  {
    key: 'key', data: 'The quick brown fox jumps over the lazy dog',
    hmac: {  // HMACs lifted from Wikipedia.
      md5: '80070713463e7749b90c2dc24911e275',
      sha1: 'de7c9b85b8b78aa6bc8a7a36f70a90701c9db4d9',
      sha256:
          'f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc' +
          '2d1a3cd8'
    }
  },
  {
    key: 'key', data: '',
    hmac: {  // Intermediate test to help debugging.
      md5: '63530468a04e386459855da0063b6596',
      sha1: 'f42bb0eeb018ebbd4597ae7213711ec60760843f',
      sha256:
          '5d5d139563c95b5967b9bd9a8c9b233a9dedb45072794cd232dc1b74' +
          '832607d0'
    }
  },
  {
    key: '', data: 'The quick brown fox jumps over the lazy dog',
    hmac: {  // Intermediate test to help debugging.
      md5: 'ad262969c53bc16032f160081c4a07a0',
      sha1: '2ba7f707ad5f187c412de3106583c3111d668de8',
      sha256:
          'fb011e6154a19b9a4c767373c305275a5a69e8b68b0b4c9200c383dc' +
          'ed19a416'
    }
  },
  {
    key: '', data: '',
    hmac: {  // HMACs lifted from Wikipedia.
      md5: '74e6f7298a9c2d168935f58c001bad88',
      sha1: 'fbdb1d1b18aa6c08324b7d64b71fb76370690e1d',
      sha256:
          'b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c71214' +
          '4292c5ad'
    }
  },
]

for (var i = 0, l = wikipedia.length; i < l; i++) {
  for (var hash in wikipedia[i]['hmac']) {
    var result = crypto.createHmac(hash, wikipedia[i]['key'])
                     .update(wikipedia[i]['data'])
                     .digest('hex');
    assert.equal(wikipedia[i]['hmac'][hash],
                 result,
                 'Test HMAC-' + hash + ': Test case ' + (i + 1) + ' wikipedia');
  }
}


// Test HMAC-SHA-* (rfc 4231 Test Cases)
var rfc4231 = [
  {
    key: new Buffer('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b', 'hex'),
    data: new Buffer('4869205468657265', 'hex'), // 'Hi There'
    hmac: {
      sha224: '896fb1128abbdf196832107cd49df33f47b4b1169912ba4f53684b22',
      sha256:
          'b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c' +
          '2e32cff7',
      sha384:
          'afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c' +
          '7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6',
      sha512:
          '87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b305' +
          '45e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f170' +
          '2e696c203a126854'
    }
  },
  {
    key: new Buffer('4a656665', 'hex'), // 'Jefe'
    data: new Buffer('7768617420646f2079612077616e7420666f72206e6f74686' +
                     '96e673f', 'hex'), // 'what do ya want for nothing?'
    hmac: {
      sha224: 'a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44',
      sha256:
          '5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b9' +
          '64ec3843',
      sha384:
          'af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec373' +
          '6322445e8e2240ca5e69e2c78b3239ecfab21649',
      sha512:
          '164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7' +
          'ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b' +
          '636e070a38bce737'
    }
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 'hex'),
    data: new Buffer('ddddddddddddddddddddddddddddddddddddddddddddddddd' +
                     'ddddddddddddddddddddddddddddddddddddddddddddddddddd',
                     'hex'),
    hmac: {
      sha224: '7fb3cb3588c6c1f6ffa9694d7d6ad2649365b0c1f65d69d1ec8333ea',
      sha256:
          '773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514' +
          'ced565fe',
      sha384:
          '88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e5' +
          '5966144b2a5ab39dc13814b94e3ab6e101a34f27',
      sha512:
          'fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33' +
          'b2279d39bf3e848279a722c806b485a47e67c807b946a337bee89426' +
          '74278859e13292fb'
    }
  },
  {
    key: new Buffer('0102030405060708090a0b0c0d0e0f10111213141516171819',
                    'hex'),
    data: new Buffer('cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdc' +
                     'dcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd',
                     'hex'),
    hmac: {
      sha224: '6c11506874013cac6a2abc1bb382627cec6a90d86efc012de7afec5a',
      sha256:
          '82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff4' +
          '6729665b',
      sha384:
          '3e8a69b7783c25851933ab6290af6ca77a9981480850009cc5577c6e' +
          '1f573b4e6801dd23c4a7d679ccf8a386c674cffb',
      sha512:
          'b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050' +
          '361ee3dba91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2d' +
          'e2adebeb10a298dd'
    }
  },

  {
    key: new Buffer('0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c', 'hex'),
    // 'Test With Truncation'
    data: new Buffer('546573742057697468205472756e636174696f6e', 'hex'),
    hmac: {
      sha224: '0e2aea68a90c8d37c988bcdb9fca6fa8',
      sha256: 'a3b6167473100ee06e0c796c2955552b',
      sha384: '3abf34c3503b2a23a46efc619baef897',
      sha512: '415fad6271580a531d4179bc891d87a6'
    },
    truncate: true
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaa', 'hex'),
    // 'Test Using Larger Than Block-Size Key - Hash Key First'
    data: new Buffer('54657374205573696e67204c6172676572205468616e20426' +
                     'c6f636b2d53697a65204b6579202d2048617368204b657920' +
                     '4669727374', 'hex'),
    hmac: {
      sha224: '95e9a0db962095adaebe9b2d6f0dbce2d499f112f2d2b7273fa6870e',
      sha256:
          '60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f' +
          '0ee37f54',
      sha384:
          '4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05' +
          '033ac4c60c2ef6ab4030fe8296248df163f44952',
      sha512:
          '80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b0137' +
          '83f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec' +
          '8b915a985d786598'
    }
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaa', 'hex'),
    // 'This is a test using a larger than block-size key and a larger ' +
    // 'than block-size data. The key needs to be hashed before being ' +
    // 'used by the HMAC algorithm.'
    data: new Buffer('5468697320697320612074657374207573696e672061206c6' +
                     '172676572207468616e20626c6f636b2d73697a65206b6579' +
                     '20616e642061206c6172676572207468616e20626c6f636b2' +
                     'd73697a6520646174612e20546865206b6579206e65656473' +
                     '20746f20626520686173686564206265666f7265206265696' +
                     'e6720757365642062792074686520484d414320616c676f72' +
                     '6974686d2e', 'hex'),
    hmac: {
      sha224: '3a854166ac5d9f023f54d517d0b39dbd946770db9c2b95c9f6f565d1',
      sha256:
          '9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f5153' +
          '5c3a35e2',
      sha384:
          '6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82' +
          '461e99c5a678cc31e799176d3860e6110c46523e',
      sha512:
          'e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d' +
          '20cdc944b6022cac3c4982b10d5eeb55c3e4de15134676fb6de04460' +
          '65c97440fa8c6a58'
    }
  }
];

for (var i = 0, l = rfc4231.length; i < l; i++) {
  for (var hash in rfc4231[i]['hmac']) {
    var str = crypto.createHmac(hash, rfc4231[i].key);
    str.end(rfc4231[i].data);
    var strRes = str.read().toString('hex');
    var result = crypto.createHmac(hash, rfc4231[i]['key'])
                     .update(rfc4231[i]['data'])
                     .digest('hex');
    if (rfc4231[i]['truncate']) {
      result = result.substr(0, 32); // first 128 bits == 32 hex chars
      strRes = strRes.substr(0, 32);
    }
    assert.equal(rfc4231[i]['hmac'][hash],
                 result,
                 'Test HMAC-' + hash + ': Test case ' + (i + 1) + ' rfc 4231');
    assert.equal(strRes, result, 'Should get same result from stream');
  }
}

// Test HMAC-MD5/SHA1 (rfc 2202 Test Cases)
var rfc2202_md5 = [
  {
    key: new Buffer('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b', 'hex'),
    data: 'Hi There',
    hmac: '9294727a3638bb1c13f48ef8158bfc9d'
  },
  {
    key: 'Jefe',
    data: 'what do ya want for nothing?',
    hmac: '750c783e6ab0b503eaa86e310a5db738'
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 'hex'),
    data: new Buffer('ddddddddddddddddddddddddddddddddddddddddddddddddd' +
                     'ddddddddddddddddddddddddddddddddddddddddddddddddddd',
                     'hex'),
    hmac: '56be34521d144c88dbb8c733f0e8b3f6'
  },
  {
    key: new Buffer('0102030405060708090a0b0c0d0e0f10111213141516171819',
                    'hex'),
    data: new Buffer('cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdc' +
                     'dcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd' +
                     'cdcdcdcdcd',
                     'hex'),
    hmac: '697eaf0aca3a3aea3a75164746ffaa79'
  },
  {
    key: new Buffer('0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c', 'hex'),
    data: 'Test With Truncation',
    hmac: '56461ef2342edc00f9bab995690efd4c'
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaa',
                    'hex'),
    data: 'Test Using Larger Than Block-Size Key - Hash Key First',
    hmac: '6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd'
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaa',
                    'hex'),
    data:
        'Test Using Larger Than Block-Size Key and Larger Than One ' +
        'Block-Size Data',
    hmac: '6f630fad67cda0ee1fb1f562db3aa53e'
  }
];
var rfc2202_sha1 = [
  {
    key: new Buffer('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b', 'hex'),
    data: 'Hi There',
    hmac: 'b617318655057264e28bc0b6fb378c8ef146be00'
  },
  {
    key: 'Jefe',
    data: 'what do ya want for nothing?',
    hmac: 'effcdf6ae5eb2fa2d27416d5f184df9c259a7c79'
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 'hex'),
    data: new Buffer('ddddddddddddddddddddddddddddddddddddddddddddd' +
                     'ddddddddddddddddddddddddddddddddddddddddddddd' +
                     'dddddddddd',
                     'hex'),
    hmac: '125d7342b9ac11cd91a39af48aa17b4f63f175d3'
  },
  {
    key: new Buffer('0102030405060708090a0b0c0d0e0f10111213141516171819',
                    'hex'),
    data: new Buffer('cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdc' +
                     'dcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd' +
                     'cdcdcdcdcd',
                     'hex'),
    hmac: '4c9007f4026250c6bc8414f9bf50c86c2d7235da'
  },
  {
    key: new Buffer('0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c', 'hex'),
    data: 'Test With Truncation',
    hmac: '4c1a03424b55e07fe7f27be1d58bb9324a9a5a04'
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaa',
                    'hex'),
    data: 'Test Using Larger Than Block-Size Key - Hash Key First',
    hmac: 'aa4ae5e15272d00e95705637ce8a3b55ed402112'
  },
  {
    key: new Buffer('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' +
                    'aaaaaaaaaaaaaaaaaaaaaa',
                    'hex'),
    data:
        'Test Using Larger Than Block-Size Key and Larger Than One ' +
        'Block-Size Data',
    hmac: 'e8e99d0f45237d786d6bbaa7965c7808bbff1a91'
  }
];

for (var i = 0, l = rfc2202_md5.length; i < l; i++) {
  assert.equal(rfc2202_md5[i]['hmac'],
               crypto.createHmac('md5', rfc2202_md5[i]['key'])
                   .update(rfc2202_md5[i]['data'])
                   .digest('hex'),
               'Test HMAC-MD5 : Test case ' + (i + 1) + ' rfc 2202');
}
for (var i = 0, l = rfc2202_sha1.length; i < l; i++) {
  assert.equal(rfc2202_sha1[i]['hmac'],
               crypto.createHmac('sha1', rfc2202_sha1[i]['key'])
                   .update(rfc2202_sha1[i]['data'])
                   .digest('hex'),
               'Test HMAC-SHA1 : Test case ' + (i + 1) + ' rfc 2202');
}

// Test hashing
var a0 = crypto.createHash('sha1').update('Test123').digest('hex');
var a1 = crypto.createHash('md5').update('Test123').digest('binary');
var a2 = crypto.createHash('sha256').update('Test123').digest('base64');
var a3 = crypto.createHash('sha512').update('Test123').digest(); // binary
var a4 = crypto.createHash('sha1').update('Test123').digest('buffer');

// stream interface
var a5 = crypto.createHash('sha512');
a5.end('Test123');
a5 = a5.read();

var a6 = crypto.createHash('sha512');
a6.write('Te');
a6.write('st');
a6.write('123');
a6.end();
a6 = a6.read();

assert.equal(a0, '8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'Test SHA1');
assert.equal(a1, 'h\u00ea\u00cb\u0097\u00d8o\fF!\u00fa+\u000e\u0017\u00ca' +
             '\u00bd\u008c', 'Test MD5 as binary');
assert.equal(a2, '2bX1jws4GYKTlxhloUB09Z66PoJZW+y+hq5R8dnx9l4=',
             'Test SHA256 as base64');
assert.deepEqual(
  a3,
  new Buffer(
    '\u00c1(4\u00f1\u0003\u001fd\u0097!O\'\u00d4C/&Qz\u00d4' +
    '\u0094\u0015l\u00b8\u008dQ+\u00db\u001d\u00c4\u00b5}\u00b2' +
    '\u00d6\u0092\u00a3\u00df\u00a2i\u00a1\u009b\n\n*\u000f' +
    '\u00d7\u00d6\u00a2\u00a8\u0085\u00e3<\u0083\u009c\u0093' +
    '\u00c2\u0006\u00da0\u00a1\u00879(G\u00ed\'',
    'binary'),
  'Test SHA512 as assumed buffer');
assert.deepEqual(a4,
                 new Buffer('8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'hex'),
                 'Test SHA1');

// stream interface should produce the same result.
assert.deepEqual(a5, a3, 'stream interface is consistent');
assert.deepEqual(a6, a3, 'stream interface is consistent');

// Test multiple updates to same hash
var h1 = crypto.createHash('sha1').update('Test123').digest('hex');
var h2 = crypto.createHash('sha1').update('Test').update('123').digest('hex');
assert.equal(h1, h2, 'multipled updates');

// Test hashing for binary files
var fn = path.join(common.fixturesDir, 'sample.png');
var sha1Hash = crypto.createHash('sha1');
var fileStream = fs.createReadStream(fn);
fileStream.on('data', function(data) {
  sha1Hash.update(data);
});
fileStream.on('close', function() {
  assert.equal(sha1Hash.digest('hex'),
               '22723e553129a336ad96e10f6aecdf0f45e4149e',
               'Test SHA1 of sample.png');
});

// Issue #2227: unknown digest method should throw an error.
assert.throws(function() {
  crypto.createHash('xyzzy');
});

// Test signing and verifying
var s1 = crypto.createSign('RSA-SHA1')
               .update('Test123')
               .sign(keyPem, 'base64');
var s1stream = crypto.createSign('RSA-SHA1');
s1stream.end('Test123');
s1stream = s1stream.sign(keyPem, 'base64');
assert.equal(s1, s1stream, 'Stream produces same output');

var verified = crypto.createVerify('RSA-SHA1')
                     .update('Test')
                     .update('123')
                     .verify(certPem, s1, 'base64');
assert.strictEqual(verified, true, 'sign and verify (base 64)');

var s2 = crypto.createSign('RSA-SHA256')
               .update('Test123')
               .sign(keyPem, 'binary');
var s2stream = crypto.createSign('RSA-SHA256');
s2stream.end('Test123');
s2stream = s2stream.sign(keyPem, 'binary');
assert.equal(s2, s2stream, 'Stream produces same output');

var verified = crypto.createVerify('RSA-SHA256')
                     .update('Test')
                     .update('123')
                     .verify(certPem, s2, 'binary');
assert.strictEqual(verified, true, 'sign and verify (binary)');

var verStream = crypto.createVerify('RSA-SHA256');
verStream.write('Tes');
verStream.write('t12');
verStream.end('3');
verified = verStream.verify(certPem, s2, 'binary');
assert.strictEqual(verified, true, 'sign and verify (stream)');

var s3 = crypto.createSign('RSA-SHA1')
               .update('Test123')
               .sign(keyPem, 'buffer');
var verified = crypto.createVerify('RSA-SHA1')
                     .update('Test')
                     .update('123')
                     .verify(certPem, s3);
assert.strictEqual(verified, true, 'sign and verify (buffer)');

var verStream = crypto.createVerify('RSA-SHA1');
verStream.write('Tes');
verStream.write('t12');
verStream.end('3');
verified = verStream.verify(certPem, s3);
assert.strictEqual(verified, true, 'sign and verify (stream)');


function testCipher1(key) {
  // Test encryption and decryption
  var plaintext = 'Keep this a secret? No! Tell everyone about node.js!';
  var cipher = crypto.createCipher('aes192', key);

  // encrypt plaintext which is in utf8 format
  // to a ciphertext which will be in hex
  var ciph = cipher.update(plaintext, 'utf8', 'hex');
  // Only use binary or hex, not base64.
  ciph += cipher.final('hex');

  var decipher = crypto.createDecipher('aes192', key);
  var txt = decipher.update(ciph, 'hex', 'utf8');
  txt += decipher.final('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption');

  // streaming cipher interface
  // NB: In real life, it's not guaranteed that you can get all of it
  // in a single read() like this.  But in this case, we know it's
  // quite small, so there's no harm.
  var cStream = crypto.createCipher('aes192', key);
  cStream.end(plaintext);
  ciph = cStream.read();

  var dStream = crypto.createDecipher('aes192', key);
  dStream.end(ciph);
  txt = dStream.read().toString('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption with streams');
}


function testCipher2(key) {
  // encryption and decryption with Base64
  // reported in https://github.com/joyent/node/issues/738
  var plaintext =
      '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
      'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
      'jAfaFg**';
  var cipher = crypto.createCipher('aes256', key);

  // encrypt plaintext which is in utf8 format
  // to a ciphertext which will be in Base64
  var ciph = cipher.update(plaintext, 'utf8', 'base64');
  ciph += cipher.final('base64');

  var decipher = crypto.createDecipher('aes256', key);
  var txt = decipher.update(ciph, 'base64', 'utf8');
  txt += decipher.final('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption with Base64');
}


function testCipher3(key, iv) {
  // Test encyrption and decryption with explicit key and iv
  var plaintext =
      '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
      'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
      'jAfaFg**';
  var cipher = crypto.createCipheriv('des-ede3-cbc', key, iv);
  var ciph = cipher.update(plaintext, 'utf8', 'hex');
  ciph += cipher.final('hex');

  var decipher = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  var txt = decipher.update(ciph, 'hex', 'utf8');
  txt += decipher.final('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption with key and iv');

  // streaming cipher interface
  // NB: In real life, it's not guaranteed that you can get all of it
  // in a single read() like this.  But in this case, we know it's
  // quite small, so there's no harm.
  var cStream = crypto.createCipheriv('des-ede3-cbc', key, iv);
  cStream.end(plaintext);
  ciph = cStream.read();

  var dStream = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  dStream.end(ciph);
  txt = dStream.read().toString('utf8');

  assert.equal(txt, plaintext, 'streaming cipher iv');
}


function testCipher4(key, iv) {
  // Test encyrption and decryption with explicit key and iv
  var plaintext =
      '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
      'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJ' +
      'jAfaFg**';
  var cipher = crypto.createCipheriv('des-ede3-cbc', key, iv);
  var ciph = cipher.update(plaintext, 'utf8', 'buffer');
  ciph = Buffer.concat([ciph, cipher.final('buffer')]);

  var decipher = crypto.createDecipheriv('des-ede3-cbc', key, iv);
  var txt = decipher.update(ciph, 'buffer', 'utf8');
  txt += decipher.final('utf8');

  assert.equal(txt, plaintext, 'encryption and decryption with key and iv');
}


testCipher1('MySecretKey123');
testCipher1(new Buffer('MySecretKey123'));

testCipher2('0123456789abcdef');
testCipher2(new Buffer('0123456789abcdef'));

testCipher3('0123456789abcd0123456789', '12345678');
testCipher3('0123456789abcd0123456789', new Buffer('12345678'));
testCipher3(new Buffer('0123456789abcd0123456789'), '12345678');
testCipher3(new Buffer('0123456789abcd0123456789'), new Buffer('12345678'));

testCipher4(new Buffer('0123456789abcd0123456789'), new Buffer('12345678'));


// update() should only take buffers / strings
assert.throws(function() {
  crypto.createHash('sha1').update({foo: 'bar'});
}, /buffer/);


// Test Diffie-Hellman with two parties sharing a secret,
// using various encodings as we go along
var dh1 = crypto.createDiffieHellman(256);
var p1 = dh1.getPrime('buffer');
var dh2 = crypto.createDiffieHellman(p1, 'base64');
var key1 = dh1.generateKeys();
var key2 = dh2.generateKeys('hex');
var secret1 = dh1.computeSecret(key2, 'hex', 'base64');
var secret2 = dh2.computeSecret(key1, 'binary', 'buffer');

assert.equal(secret1, secret2.toString('base64'));

// Create "another dh1" using generated keys from dh1,
// and compute secret again
var dh3 = crypto.createDiffieHellman(p1, 'buffer');
var privkey1 = dh1.getPrivateKey();
dh3.setPublicKey(key1);
dh3.setPrivateKey(privkey1);

assert.deepEqual(dh1.getPrime(), dh3.getPrime());
assert.deepEqual(dh1.getGenerator(), dh3.getGenerator());
assert.deepEqual(dh1.getPublicKey(), dh3.getPublicKey());
assert.deepEqual(dh1.getPrivateKey(), dh3.getPrivateKey());

var secret3 = dh3.computeSecret(key2, 'hex', 'base64');

assert.equal(secret1, secret3);

assert.throws(function() {
  dh3.computeSecret('');
}, /key is too small/i);

// Create a shared using a DH group.
var alice = crypto.createDiffieHellmanGroup('modp5');
var bob = crypto.createDiffieHellmanGroup('modp5');
alice.generateKeys();
bob.generateKeys();
var aSecret = alice.computeSecret(bob.getPublicKey()).toString('hex');
var bSecret = bob.computeSecret(alice.getPublicKey()).toString('hex');
assert.equal(aSecret, bSecret);


// https://github.com/joyent/node/issues/2338
assert.throws(function() {
  var p = 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74' +
          '020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437' +
          '4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED' +
          'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381FFFFFFFFFFFFFFFF';
  crypto.createDiffieHellman(p, 'hex');
});

// Test RSA key signing/verification
var rsaSign = crypto.createSign('RSA-SHA1');
var rsaVerify = crypto.createVerify('RSA-SHA1');
assert.ok(rsaSign);
assert.ok(rsaVerify);

rsaSign.update(rsaPubPem);
var rsaSignature = rsaSign.sign(rsaKeyPem, 'hex');
assert.equal(rsaSignature,
             '5c50e3145c4e2497aadb0eabc83b342d0b0021ece0d4c4a064b7c' +
             '8f020d7e2688b122bfb54c724ac9ee169f83f66d2fe90abeb95e8' +
             'e1290e7e177152a4de3d944cf7d4883114a20ed0f78e70e25ef0f' +
             '60f06b858e6af42a2f276ede95bbc6bc9a9bbdda15bd663186a6f' +
             '40819a7af19e577bb2efa5e579a1f5ce8a0d4ca8b8f6');

rsaVerify.update(rsaPubPem);
assert.strictEqual(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), true);


//
// Test RSA signing and verification
//
(function() {
  var privateKey = fs.readFileSync(
      common.fixturesDir + '/test_rsa_privkey_2.pem');

  var publicKey = fs.readFileSync(
      common.fixturesDir + '/test_rsa_pubkey_2.pem');

  var input = 'I AM THE WALRUS';

  var signature =
      '79d59d34f56d0e94aa6a3e306882b52ed4191f07521f25f505a078dc2f89' +
      '396e0c8ac89e996fde5717f4cb89199d8fec249961fcb07b74cd3d2a4ffa' +
      '235417b69618e4bcd76b97e29975b7ce862299410e1b522a328e44ac9bb2' +
      '8195e0268da7eda23d9825ac43c724e86ceeee0d0d4465678652ccaf6501' +
      '0ddfb299bedeb1ad';

  var sign = crypto.createSign('RSA-SHA256');
  sign.update(input);

  var output = sign.sign(privateKey, 'hex');
  assert.equal(output, signature);

  var verify = crypto.createVerify('RSA-SHA256');
  verify.update(input);

  assert.strictEqual(verify.verify(publicKey, signature, 'hex'), true);
})();


//
// Test DSA signing and verification
//
(function() {
  var privateKey = fs.readFileSync(
      common.fixturesDir + '/test_dsa_privkey.pem');

  var publicKey = fs.readFileSync(
      common.fixturesDir + '/test_dsa_pubkey.pem');

  var input = 'I AM THE WALRUS';

  // DSA signatures vary across runs so there is no static string to verify
  // against
  var sign = crypto.createSign('DSS1');
  sign.update(input);
  var signature = sign.sign(privateKey, 'hex');

  var verify = crypto.createVerify('DSS1');
  verify.update(input);

  assert.strictEqual(verify.verify(publicKey, signature, 'hex'), true);
})();


//
// Test PBKDF2 with RFC 6070 test vectors (except #4)
//
function testPBKDF2(password, salt, iterations, keylen, expected) {
  var actual = crypto.pbkdf2Sync(password, salt, iterations, keylen);
  assert.equal(actual.toString('binary'), expected);

  crypto.pbkdf2(password, salt, iterations, keylen, function(err, actual) {
    assert.equal(actual.toString('binary'), expected);
  });
}


testPBKDF2('password', 'salt', 1, 20,
           '\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9\xb5\x24' +
           '\xaf\x60\x12\x06\x2f\xe0\x37\xa6');

testPBKDF2('password', 'salt', 2, 20,
           '\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e\xd9\x2a' +
           '\xce\x1d\x41\xf0\xd8\xde\x89\x57');

testPBKDF2('password', 'salt', 4096, 20,
           '\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad\x49\xd9\x26' +
           '\xf7\x21\xd0\x65\xa4\x29\xc1');

testPBKDF2('passwordPASSWORDpassword',
           'saltSALTsaltSALTsaltSALTsaltSALTsalt',
           4096,
           25,
           '\x3d\x2e\xec\x4f\xe4\x1c\x84\x9b\x80\xc8\xd8\x36\x62' +
           '\xc0\xe4\x4a\x8b\x29\x1a\x96\x4c\xf2\xf0\x70\x38');

testPBKDF2('pass\0word', 'sa\0lt', 4096, 16,
           '\x56\xfa\x6a\xa7\x55\x48\x09\x9d\xcc\x37\xd7\xf0\x34' +
           '\x25\xe0\xc3');

function assertSorted(list) {
  assert.deepEqual(list, list.sort());
}

// Assume that we have at least AES-128-CBC.
assert.notEqual(0, crypto.getCiphers().length);
assert.notEqual(-1, crypto.getCiphers().indexOf('aes-128-cbc'));
assert.equal(-1, crypto.getCiphers().indexOf('AES-128-CBC'));
assertSorted(crypto.getCiphers());

// Assume that we have at least AES256-SHA.
var tls = require('tls');
assert.notEqual(0, tls.getCiphers().length);
assert.notEqual(-1, tls.getCiphers().indexOf('aes256-sha'));
assert.equal(-1, tls.getCiphers().indexOf('AES256-SHA'));
assertSorted(tls.getCiphers());

// Assert that we have sha and sha1 but not SHA and SHA1.
assert.notEqual(0, crypto.getHashes().length);
assert.notEqual(-1, crypto.getHashes().indexOf('sha1'));
assert.notEqual(-1, crypto.getHashes().indexOf('sha'));
assert.equal(-1, crypto.getHashes().indexOf('SHA1'));
assert.equal(-1, crypto.getHashes().indexOf('SHA'));
assertSorted(crypto.getHashes());

(function() {
  var c = crypto.createDecipher('aes-128-ecb', '');
  assert.throws(function() { c.final('utf8') }, /invalid public key/);
})();

// Base64 padding regression test, see #4837.
(function() {
  var c = crypto.createCipher('aes-256-cbc', 'secret');
  var s = c.update('test', 'utf8', 'base64') + c.final('base64');
  assert.equal(s, '375oxUQCIocvxmC5At+rvA==');
})();

// Error path should not leak memory (check with valgrind).
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, 20, null);
});

// Calling Cipher.final() or Decipher.final() twice should error but
// not assert. See #4886.
(function() {
  var c = crypto.createCipher('aes-256-cbc', 'secret');
  try { c.final('xxx') } catch (e) { /* Ignore. */ }
  try { c.final('xxx') } catch (e) { /* Ignore. */ }
  try { c.final('xxx') } catch (e) { /* Ignore. */ }
  var d = crypto.createDecipher('aes-256-cbc', 'secret');
  try { d.final('xxx') } catch (e) { /* Ignore. */ }
  try { d.final('xxx') } catch (e) { /* Ignore. */ }
  try { d.final('xxx') } catch (e) { /* Ignore. */ }
})();
