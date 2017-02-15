'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const cacert =
`-----BEGIN CERTIFICATE-----
MIIBxTCCAX8CAnXnMA0GCSqGSIb3DQEBBQUAMH0xCzAJBgNVBAYTAlVTMQswCQYD
VQQIEwJDQTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEZMBcGA1UEChMQU3Ryb25n
TG9vcCwgSW5jLjESMBAGA1UECxMJU3Ryb25nT3BzMRowGAYDVQQDExFjYS5zdHJv
bmdsb29wLmNvbTAeFw0xNDAxMTcyMjE1MDdaFw00MTA2MDMyMjE1MDdaMH0xCzAJ
BgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEZ
MBcGA1UEChMQU3Ryb25nTG9vcCwgSW5jLjESMBAGA1UECxMJU3Ryb25nT3BzMRow
GAYDVQQDExFjYS5zdHJvbmdsb29wLmNvbTBMMA0GCSqGSIb3DQEBAQUAAzsAMDgC
MQDKbQ6rIR5t1q1v4Ha36jrq0IkyUohy9EYNvLnXUly1PGqxby0ILlAVJ8JawpY9
AVkCAwEAATANBgkqhkiG9w0BAQUFAAMxALA1uS4CqQXRSAyYTfio5oyLGz71a+NM
+0AFLBwh5AQjhGd0FcenU4OfHxyDEOJT/Q==
-----END CERTIFICATE-----`;

const cert =
`-----BEGIN CERTIFICATE-----
MIIBfDCCATYCAgQaMA0GCSqGSIb3DQEBBQUAMH0xCzAJBgNVBAYTAlVTMQswCQYD
VQQIEwJDQTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEZMBcGA1UEChMQU3Ryb25n
TG9vcCwgSW5jLjESMBAGA1UECxMJU3Ryb25nT3BzMRowGAYDVQQDExFjYS5zdHJv
bmdsb29wLmNvbTAeFw0xNDAxMTcyMjE1MDdaFw00MTA2MDMyMjE1MDdaMBkxFzAV
BgNVBAMTDnN0cm9uZ2xvb3AuY29tMEwwDQYJKoZIhvcNAQEBBQADOwAwOAIxAMfk
I0LWU15pPUwIQNMnRVhhOibi0TQmAau8FBtgwEfGK01WpfGUaJr1a41K8Uq7xwID
AQABoxkwFzAVBgNVHREEDjAMhwQAAAAAhwR/AAABMA0GCSqGSIb3DQEBBQUAAzEA
cGpYrhkrb7mIh9DNhV0qp7pGjqBzlHqB7KQXw2luLDp//6dyHBMexDCQznkhZKRU
-----END CERTIFICATE-----`;

const key =
`-----BEGIN RSA PRIVATE KEY-----
MIH0AgEAAjEAx+QjQtZTXmk9TAhA0ydFWGE6JuLRNCYBq7wUG2DAR8YrTVal8ZRo
mvVrjUrxSrvHAgMBAAECMBCGccvSwC2r8Z9Zh1JtirQVxaL1WWpAQfmVwLe0bAgg
/JWMU/6hS36TsYyZMxwswQIZAPTAfht/zDLb7Hwgu2twsS1Ra9w/yyvtlwIZANET
26votwJAHK1yUrZGA5nnp5qcmQ/JUQIZAII5YV/UUZvF9D/fUplJ7puENPWNY9bN
pQIZAMMwxuS3XiO7two2sQF6W+JTYyX1DPCwAQIZAOYg1TvEGT38k8e8jygv8E8w
YqrWTeQFNQ==
-----END RSA PRIVATE KEY-----`;

const ca = [ cert, cacert ];

let clientError = null;
let connectError = null;

const server = tls.createServer({ ca: ca, cert: cert, key: key }, () => {
  common.fail('should be unreachable');
}).on('tlsClientError', function(err, conn) {
  assert(!clientError && conn);
  clientError = err;
}).listen(0, function() {
  const options = {
    ciphers: 'AES128-GCM-SHA256',
    port: this.address().port,
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
