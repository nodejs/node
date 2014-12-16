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

var common = require('../common');
var assert = require('assert');
var tls = require('tls');

var cacert = '-----BEGIN CERTIFICATE-----\n' +
  'MIIBxTCCAX8CAnXnMA0GCSqGSIb3DQEBBQUAMH0xCzAJBgNVBAYTAlVTMQswCQYD\n' +
  'VQQIEwJDQTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEZMBcGA1UEChMQU3Ryb25n\n' +
  'TG9vcCwgSW5jLjESMBAGA1UECxMJU3Ryb25nT3BzMRowGAYDVQQDExFjYS5zdHJv\n' +
  'bmdsb29wLmNvbTAeFw0xNDAxMTcyMjE1MDdaFw00MTA2MDMyMjE1MDdaMH0xCzAJ\n' +
  'BgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEZ\n' +
  'MBcGA1UEChMQU3Ryb25nTG9vcCwgSW5jLjESMBAGA1UECxMJU3Ryb25nT3BzMRow\n' +
  'GAYDVQQDExFjYS5zdHJvbmdsb29wLmNvbTBMMA0GCSqGSIb3DQEBAQUAAzsAMDgC\n' +
  'MQDKbQ6rIR5t1q1v4Ha36jrq0IkyUohy9EYNvLnXUly1PGqxby0ILlAVJ8JawpY9\n' +
  'AVkCAwEAATANBgkqhkiG9w0BAQUFAAMxALA1uS4CqQXRSAyYTfio5oyLGz71a+NM\n' +
  '+0AFLBwh5AQjhGd0FcenU4OfHxyDEOJT/Q==\n' +
  '-----END CERTIFICATE-----\n';

var cert = '-----BEGIN CERTIFICATE-----\n' +
  'MIIBfDCCATYCAgQaMA0GCSqGSIb3DQEBBQUAMH0xCzAJBgNVBAYTAlVTMQswCQYD\n' +
  'VQQIEwJDQTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEZMBcGA1UEChMQU3Ryb25n\n' +
  'TG9vcCwgSW5jLjESMBAGA1UECxMJU3Ryb25nT3BzMRowGAYDVQQDExFjYS5zdHJv\n' +
  'bmdsb29wLmNvbTAeFw0xNDAxMTcyMjE1MDdaFw00MTA2MDMyMjE1MDdaMBkxFzAV\n' +
  'BgNVBAMTDnN0cm9uZ2xvb3AuY29tMEwwDQYJKoZIhvcNAQEBBQADOwAwOAIxAMfk\n' +
  'I0LWU15pPUwIQNMnRVhhOibi0TQmAau8FBtgwEfGK01WpfGUaJr1a41K8Uq7xwID\n' +
  'AQABoxkwFzAVBgNVHREEDjAMhwQAAAAAhwR/AAABMA0GCSqGSIb3DQEBBQUAAzEA\n' +
  'cGpYrhkrb7mIh9DNhV0qp7pGjqBzlHqB7KQXw2luLDp//6dyHBMexDCQznkhZKRU\n' +
  '-----END CERTIFICATE-----\n';

var key = '-----BEGIN RSA PRIVATE KEY-----\n' +
  'MIH0AgEAAjEAx+QjQtZTXmk9TAhA0ydFWGE6JuLRNCYBq7wUG2DAR8YrTVal8ZRo\n' +
  'mvVrjUrxSrvHAgMBAAECMBCGccvSwC2r8Z9Zh1JtirQVxaL1WWpAQfmVwLe0bAgg\n' +
  '/JWMU/6hS36TsYyZMxwswQIZAPTAfht/zDLb7Hwgu2twsS1Ra9w/yyvtlwIZANET\n' +
  '26votwJAHK1yUrZGA5nnp5qcmQ/JUQIZAII5YV/UUZvF9D/fUplJ7puENPWNY9bN\n' +
  'pQIZAMMwxuS3XiO7two2sQF6W+JTYyX1DPCwAQIZAOYg1TvEGT38k8e8jygv8E8w\n' +
  'YqrWTeQFNQ==\n' +
  '-----END RSA PRIVATE KEY-----\n';

var ca = [ cert, cacert ];

var clientError = null;
var connectError = null;

var server = tls.createServer({ ca: ca, cert: cert, key: key }, function(conn) {
  throw 'unreachable';
}).on('clientError', function(err, conn) {
  assert(!clientError && conn);
  clientError = err;
}).listen(common.PORT, function() {
  var options = {
    ciphers: 'AES128-GCM-SHA256',
    port: common.PORT,
    ca: ca
  };
  tls.connect(options).on('error', function(err) {
    assert(!connectError);

    connectError = err;
    this.destroy();
    server.close();
  }).write('123');
});

process.on('exit', function() {
  assert(clientError);
  assert(connectError);
  assert(/socket hang up/.test(clientError.message));
  assert(/ECONNRESET/.test(clientError.code));
});
