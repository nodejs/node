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

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

const cert =
`-----BEGIN CERTIFICATE-----
MIIDNDCCAp2gAwIBAgIJAJvXLQpGPpm7MA0GCSqGSIb3DQEBBQUAMHAxCzAJBgNV
BAYTAkdCMRAwDgYDVQQIEwdHd3luZWRkMREwDwYDVQQHEwhXYXVuZmF3cjEUMBIG
A1UEChMLQWNrbmFjayBMdGQxEjAQBgNVBAsTCVRlc3QgQ2VydDESMBAGA1UEAxMJ
bG9jYWxob3N0MB4XDTA5MTEwMjE5MzMwNVoXDTEwMTEwMjE5MzMwNVowcDELMAkG
A1UEBhMCR0IxEDAOBgNVBAgTB0d3eW5lZGQxETAPBgNVBAcTCFdhdW5mYXdyMRQw
EgYDVQQKEwtBY2tuYWNrIEx0ZDESMBAGA1UECxMJVGVzdCBDZXJ0MRIwEAYDVQQD
Ewlsb2NhbGhvc3QwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBANdym7nGe2yw
6LlJfJrQtC5TmKOGrSXiyolYCbGOy4xZI4KD31d3097jhlQFJyF+10gwkE62DuJe
fLvBZDUsvLe1R8bzlVhZnBVn+3QJyUIWQAL+DsRj8P3KoD7k363QN5dIaA1GOAg2
vZcPy1HCUsvOgvDXGRUCZqNLAyt+h/cpAgMBAAGjgdUwgdIwHQYDVR0OBBYEFK4s
VBV4shKUj3UX/fvSJnFaaPBjMIGiBgNVHSMEgZowgZeAFK4sVBV4shKUj3UX/fvS
JnFaaPBjoXSkcjBwMQswCQYDVQQGEwJHQjEQMA4GA1UECBMHR3d5bmVkZDERMA8G
A1UEBxMIV2F1bmZhd3IxFDASBgNVBAoTC0Fja25hY2sgTHRkMRIwEAYDVQQLEwlU
ZXN0IENlcnQxEjAQBgNVBAMTCWxvY2FsaG9zdIIJAJvXLQpGPpm7MAwGA1UdEwQF
MAMBAf8wDQYJKoZIhvcNAQEFBQADgYEAFxR7BA1mUlsYqPiogtxSIfLzHWh+s0bJ
SBuhNrHes4U8QxS8+x/KWjd/81gzsf9J1C2VzTlFaydAgigz3SkQYgs+TMnFkT2o
9jqoJrcdf4WpZ2DQXUALaZgwNzPumMUSx8Ac5gO+BY/RHyP6fCodYvdNwyKslnI3
US7eCSHZsVo=
-----END CERTIFICATE-----`;

const key =
`-----BEGIN RSA PRIVATE KEY-----
MIICXgIBAAKBgQDXcpu5xntssOi5SXya0LQuU5ijhq0l4sqJWAmxjsuMWSOCg99X
d9Pe44ZUBSchftdIMJBOtg7iXny7wWQ1LLy3tUfG85VYWZwVZ/t0CclCFkAC/g7E
Y/D9yqA+5N+t0DeXSGgNRjgINr2XD8tRwlLLzoLw1xkVAmajSwMrfof3KQIDAQAB
AoGBAIBHR/tT93ce2mJAJAXV0AJpWc+7x2pwX2FpXtQujnlxNZhnRlrBCRCD7h4m
t0bVS/86kyGaesBDvAbavfx/N5keYzzmmSp5Ht8IPqKPydGWdigk4x90yWvktai7
dWuRKF94FXr0GUuBONb/dfHdp4KBtzN7oIF9WydYGGXA9ZmBAkEA8/k01bfwQZIu
AgcdNEM94Zcug1gSspXtUu8exNQX4+PNVbadghZb1+OnUO4d3gvWfqvAnaXD3KV6
N4OtUhQQ0QJBAOIRbKMfaymQ9yE3CQQxYfKmEhHXWARXVwuYqIFqjmhSjSXx0l/P
7mSHz1I9uDvxkJev8sQgu1TKIyTOdqPH1tkCQQDPa6H1yYoj1Un0Q2Qa2Mg1kTjk
Re6vkjPQ/KcmJEOjZjtekgFbZfLzmwLXFXqjG2FjFFaQMSxR3QYJSJQEYjbhAkEA
sy7OZcjcXnjZeEkv61Pc57/7qIp/6Aj2JGnefZ1gvI1Z9Q5kCa88rA/9Iplq8pA4
ZBKAoDW1ZbJGAsFmxc/6mQJAdPilhci0qFN86IGmf+ZBnwsDflIwHKDaVofti4wQ
sPWhSOb9VQjMXekI4Y2l8fqAVTS2Fn6+8jkVKxXBywSVCw==
-----END RSA PRIVATE KEY-----`;

function test(cert, key, cb) {
  const server = tls.createServer({
    cert,
    key
  }).listen(0, function() {
    server.close(cb);
  });
}

test(cert, key, common.mustCall(function() {
  test(Buffer.from(cert), Buffer.from(key), common.mustCall());
}));
