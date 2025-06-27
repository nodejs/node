'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

// Verify that multiple CA certificates can be provided, and that for
// convenience that can also be in newline-separated strings.

const tls = require('tls');
const fixtures = require('../common/fixtures');

const ca1 = fixtures.readKey('ca1-cert.pem', 'utf8');
const ca2 = fixtures.readKey('ca2-cert.pem', 'utf8');
const cert = fixtures.readKey('agent3-cert.pem', 'utf8');
const key = fixtures.readKey('agent3-key.pem', 'utf8');

function test(ca) {
  const server = tls.createServer({ ca, cert, key });

  server.addContext('agent3', { ca, cert, key });

  const host = common.localhostIPv4;
  server.listen(0, host, common.mustCall(() => {
    const socket = tls.connect({
      servername: 'agent3',
      host,
      port: server.address().port,
      ca
    }, common.mustCall(() => {
      socket.end();
    }));

    socket.on('close', () => {
      server.close();
    });
  }));
}

// `ca1` is not actually necessary for the certificate validation -- maybe
// the fixtures should be written in a way that requires it?
test([ca1, ca2]);
test(`${ca1}\n${ca2}`);
