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

'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');


const cert =
`-----BEGIN CERTIFICATE-----
MIIBfjCCASgCCQDmmNjAojbDQjANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJB
VTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0
cyBQdHkgTHRkMCAXDTE0MDExNjE3NTMxM1oYDzIyODcxMDMxMTc1MzEzWjBFMQsw
CQYDVQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJu
ZXQgV2lkZ2l0cyBQdHkgTHRkMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPKwlfMX
6HGZIt1xm7fna72eWcOYfUfSxSugghvqYgJt2Oi3lH+wsU1O9FzRIVmpeIjDXhbp
Mjsa1HtzSiccPXsCAwEAATANBgkqhkiG9w0BAQUFAANBAHOoKy0NkyfiYH7Ne5ka
uvCyndyeB4d24FlfqEUlkfaWCZlNKRaV9YhLDiEg3BcIreFo4brtKQfZzTRs0GVm
KHg=
-----END CERTIFICATE-----`;

const key =
`-----BEGIN RSA PRIVATE KEY-----
MIIBPQIBAAJBAPKwlfMX6HGZIt1xm7fna72eWcOYfUfSxSugghvqYgJt2Oi3lH+w
sU1O9FzRIVmpeIjDXhbpMjsa1HtzSiccPXsCAwEAAQJBAM4uU9aJE0OfdE1p/X+K
LrCT3XMdFCJ24GgmHyOURtwDy18upQJecDVdcZp16fjtOPmaW95GoYRyifB3R4I5
RxECIQD7jRM9slCSVV8xp9kOJQNpHjhRQYVGBn+pyllS2sb+RQIhAPb7Y+BIccri
NWnuhwCW8hA7Fkj/kaBdAwyW7L3Tvui/AiEAiqLCovMecre4Yi6GcsQ1b/6mvSmm
IOS+AT6zIfXPTB0CIQCJKGR3ymN/Qw5crL1GQ41cHCQtF9ickOq/lBUW+j976wIh
AOaJnkQrmurlRdePX6LvN/LgGAQoxwovfjcOYNnZsIVY
-----END RSA PRIVATE KEY-----`;

function test(cert, key, cb) {
  const server = tls.createServer({
    cert: cert,
    key: key
  }).listen(0, function() {
    server.close(cb);
  });
}

test(cert, key, common.mustCall(function() {
  test(Buffer.from(cert), Buffer.from(key), common.mustCall(function() {}));
}));
