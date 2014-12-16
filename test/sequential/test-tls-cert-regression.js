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

if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var tls = require('tls');

var assert = require('assert');
var common = require('../common');

var cert = '-----BEGIN CERTIFICATE-----\n' +
  'MIIBfjCCASgCCQDmmNjAojbDQjANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJB\n' +
  'VTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0\n' +
  'cyBQdHkgTHRkMCAXDTE0MDExNjE3NTMxM1oYDzIyODcxMDMxMTc1MzEzWjBFMQsw\n' +
  'CQYDVQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJu\n' +
  'ZXQgV2lkZ2l0cyBQdHkgTHRkMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPKwlfMX\n' +
  '6HGZIt1xm7fna72eWcOYfUfSxSugghvqYgJt2Oi3lH+wsU1O9FzRIVmpeIjDXhbp\n' +
  'Mjsa1HtzSiccPXsCAwEAATANBgkqhkiG9w0BAQUFAANBAHOoKy0NkyfiYH7Ne5ka\n' +
  'uvCyndyeB4d24FlfqEUlkfaWCZlNKRaV9YhLDiEg3BcIreFo4brtKQfZzTRs0GVm\n' +
  'KHg=\n' +
  '-----END CERTIFICATE-----';
var key = '-----BEGIN RSA PRIVATE KEY-----\n' +
  'MIIBPQIBAAJBAPKwlfMX6HGZIt1xm7fna72eWcOYfUfSxSugghvqYgJt2Oi3lH+w\n' +
  'sU1O9FzRIVmpeIjDXhbpMjsa1HtzSiccPXsCAwEAAQJBAM4uU9aJE0OfdE1p/X+K\n' +
  'LrCT3XMdFCJ24GgmHyOURtwDy18upQJecDVdcZp16fjtOPmaW95GoYRyifB3R4I5\n' +
  'RxECIQD7jRM9slCSVV8xp9kOJQNpHjhRQYVGBn+pyllS2sb+RQIhAPb7Y+BIccri\n' +
  'NWnuhwCW8hA7Fkj/kaBdAwyW7L3Tvui/AiEAiqLCovMecre4Yi6GcsQ1b/6mvSmm\n' +
  'IOS+AT6zIfXPTB0CIQCJKGR3ymN/Qw5crL1GQ41cHCQtF9ickOq/lBUW+j976wIh\n' +
  'AOaJnkQrmurlRdePX6LvN/LgGAQoxwovfjcOYNnZsIVY\n' +
  '-----END RSA PRIVATE KEY-----';

function test(cert, key, cb) {
  var server = tls.createServer({
    cert: cert,
    key: key
  }).listen(common.PORT, function() {
    server.close(cb);
  });
}

var completed = false;
test(cert, key, function() {
  test(new Buffer(cert), new Buffer(key), function() {
    completed = true;
  });
});

process.on('exit', function() {
  assert(completed);
});
