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

var crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

var fs = require('fs');
var path = require('path');

// Test Certificates
var spkacValid = fs.readFileSync(common.fixturesDir + '/spkac.valid');
var spkacFail = fs.readFileSync(common.fixturesDir + '/spkac.fail');
var spkacPem = fs.readFileSync(common.fixturesDir + '/spkac.pem');

var certificate = new crypto.Certificate();

assert.equal(certificate.verifySpkac(spkacValid), true);
assert.equal(certificate.verifySpkac(spkacFail), false);

assert.equal(stripLineEndings(certificate.exportPublicKey(spkacValid)
							  .toString('utf8')),
			 stripLineEndings(spkacPem.toString('utf8')));
assert.equal(certificate.exportPublicKey(spkacFail), '');

assert.equal(certificate.exportChallenge(spkacValid)
			 .toString('utf8'),
			 'fb9ab814-6677-42a4-a60c-f905d1a6924d');
assert.equal(certificate.exportChallenge(spkacFail), '');

function stripLineEndings(obj) {
  return obj.replace(/\n/g, '');
}
