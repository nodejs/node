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

// taken from test/fixtures/keys/agent2-cert.pem
const cert = `-----BEGIN CERTIFICATE-----
MIICcTCCAdoCCQDTgzSLdDTF0TANBgkqhkiG9w0BAQUFADB9MQswCQYDVQQGEwJV
UzELMAkGA1UECBMCQ0ExCzAJBgNVBAcTAlNGMQ8wDQYDVQQKEwZKb3llbnQxEDAO
BgNVBAsTB05vZGUuanMxDzANBgNVBAMTBmFnZW50MjEgMB4GCSqGSIb3DQEJARYR
cnlAdGlueWNsb3Vkcy5vcmcwHhcNMTMwODAxMTExOTAwWhcNNDAxMjE2MTExOTAw
WjB9MQswCQYDVQQGEwJVUzELMAkGA1UECBMCQ0ExCzAJBgNVBAcTAlNGMQ8wDQYD
VQQKEwZKb3llbnQxEDAOBgNVBAsTB05vZGUuanMxDzANBgNVBAMTBmFnZW50MjEg
MB4GCSqGSIb3DQEJARYRcnlAdGlueWNsb3Vkcy5vcmcwgZ8wDQYJKoZIhvcNAQEB
BQADgY0AMIGJAoGBAKGYRnu2BdY2R8flqKPLICWO/7NoRVGH4KZBY1uBF/VYXyA2
VT5O7461mt6oA372BItGyNxdbMEvQBRcLiXTueKF5D+KYu30bWem6A/AxxYvnqU4
tP+uhsXNuGNQTp8i0vBDM/nUx7QGeP1Kda6C936PCNt7wbGPKPNyACNMbnptAgMB
AAEwDQYJKoZIhvcNAQEFBQADgYEATzjDAPocPA2Jm8wrLBW+fOC478wMo9gT3Y3N
ZU6fnF2dEPFLNETCMtDxnKhi4hnBpaiZ0fu0oaR1cSDRIVtlyW4azNjny4495C0F
JLuP5P5pz+rJe+ImKw+mO1ARA9fUAL3VN6/kVXY/EspwWJcLbJ5jdsDmkRbV52hX
Th4jkAI=
-----END CERTIFICATE-----`;

// taken from test/fixtures/keys/agent2-key.pem
const key = `-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQChmEZ7tgXWNkfH5aijyyAljv+zaEVRh+CmQWNbgRf1WF8gNlU+
Tu+OtZreqAN+9gSLRsjcXWzBL0AUXC4l07niheQ/imLt9G1npugPwMcWL56lOLT/
robFzbhjUE6fItLwQzP51Me0Bnj9SnWugvd+jwjbe8GxjyjzcgAjTG56bQIDAQAB
AoGAd19C6g5731N30T5hRqY+GCC72a90TZc/p/Fz0Vva8/4VP3mDnSS4qMaVIlgh
RP++OZjPtqI5PbiG8MNrv7vZe0UXlV7oZE0IA+jomUXsplbwMFf6pkrqdyHi+cbm
rBudhmKeLUgNA6peMGVA83C5g2SMqU5kB+tWzZT7Rs9rsyECQQDWpXxZgULqbFZv
wjpIDGWjOpQZrv123bJ9TQ+VoskCu4vlyDJqDJPwnscl8NnzpFJriDARn0WrB2sd
8GCX1yEpAkEAwLo/MYG5elkNRsE5/vINSIo04Gu6tP/Sd7EBtHYAPHUPjs/MhhVX
tMIGtACheHMwjGRPyr8pboEp2LEap4GjpQJBALNsy+CJ0+TfwPVU96EIc+GZcvlx
NMErGyvwwclEtSDKo2vmCHZrozLtlu1ZQueOgbMPuZbRe8w2vEzfhe8HTtkCQAYy
NrPlwsvPLyEWN0IeEBVD9D0+2WrWSrL0auSdYpaPAOgLgDzTVNWH42VIG+jeczIg
S3xuNuvJlUnVL9Ew1s0CQQCly+gduXtvOYip1/Stm/65kT7d8ICQgjh0XSPw/kUC
llVMQY3z1iFCaj/z0Csr0t0kJ534bH7GP3LOoNruV0p9
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
