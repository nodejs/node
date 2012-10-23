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
  console.log('Not compiled with OpenSSL support.');
  process.exit();
}

crypto.DEFAULT_ENCODING = 'buffer';

function aes256(decipherFinal) {
  var iv  = new Buffer('00000000000000000000000000000000', 'hex');
  var key = new Buffer('0123456789abcdef0123456789abcdef' +
                       '0123456789abcdef0123456789abcdef', 'hex');

  function encrypt(val, pad) {
        var c = crypto.createCipheriv('aes256', key, iv);
        c.setAutoPadding(pad);
        return c.update(val, 'utf8', 'binary') + c.final('binary');
  }

  function decrypt(val, pad) {
        var c = crypto.createDecipheriv('aes256', key, iv);
        c.setAutoPadding(pad);
        return c.update(val, 'binary', 'utf8') + c[decipherFinal]('utf8');
  }

  // echo 0123456789abcdef0123456789abcdef \
  // | openssl enc -e -aes256 -nopad -K <key> -iv <iv> \
  // | openssl enc -d -aes256 -nopad -K <key> -iv <iv>
  var plaintext = '0123456789abcdef0123456789abcdef'; // multiple of block size
  var encrypted = encrypt(plaintext, false);
  var decrypted = decrypt(encrypted, false);
  assert.equal(decrypted, plaintext);

  // echo 0123456789abcdef0123456789abcde \
  // | openssl enc -e -aes256 -K <key> -iv <iv> \
  // | openssl enc -d -aes256 -K <key> -iv <iv>
  plaintext = '0123456789abcdef0123456789abcde'; // not a multiple
  encrypted = encrypt(plaintext, true);
  decrypted = decrypt(encrypted, true);
  assert.equal(decrypted, plaintext);
}

aes256('final');
aes256('finaltol');
