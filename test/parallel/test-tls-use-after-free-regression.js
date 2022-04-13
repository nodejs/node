'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');
const tls = require('tls');

const kMessage =
  'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\n';

const key = `-----BEGIN EC PARAMETERS-----
BggqhkjOPQMBBw==
-----END EC PARAMETERS-----
-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIDKfHHbiJMdu2STyHL11fWC7psMY19/gUNpsUpkwgGACoAoGCCqGSM49
AwEHoUQDQgAEItqm+pYj3Ca8bi5mBs+H8xSMxuW2JNn4I+kw3aREsetLk8pn3o81
PWBiTdSZrGBGQSy+UAlQvYeE6Z/QXQk8aw==
-----END EC PRIVATE KEY-----`;

const cert = `-----BEGIN CERTIFICATE-----
MIIBhjCCASsCFDJU1tCo88NYU//pE+DQKO9hUDsFMAoGCCqGSM49BAMCMEUxCzAJ
BgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5l
dCBXaWRnaXRzIFB0eSBMdGQwHhcNMjAwOTIyMDg1NDU5WhcNNDgwMjA3MDg1NDU5
WjBFMQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwY
SW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcD
QgAEItqm+pYj3Ca8bi5mBs+H8xSMxuW2JNn4I+kw3aREsetLk8pn3o81PWBiTdSZ
rGBGQSy+UAlQvYeE6Z/QXQk8azAKBggqhkjOPQQDAgNJADBGAiEA7Bdn4F87KqIe
Y/ABy/XIXXpFUb2nyv3zV7POQi2lPcECIQC3UWLmfiedpiIKsf9YRIyO0uEood7+
glj2R1NNr1X68w==
-----END CERTIFICATE-----`;

const server = https.createServer(
  { key, cert },
  common.mustCall((req, res) => {
    res.writeHead(200);
    res.end('boom goes the dynamite\n');
  }, 3));

server.listen(0, common.mustCall(() => {
  const socket =
    tls.connect(
      server.address().port,
      'localhost',
      { rejectUnauthorized: false },
      common.mustCall(() => {
        socket.write(kMessage);
        socket.write(kMessage);
        socket.write(kMessage);
      }));

  socket.on('data', common.mustCall(() => socket.destroy()));
  socket.on('close', () => {
    setImmediate(() => server.close());
  });
}));
