'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Test that SNI context matching is case-insensitive, as required by
// RFC 6066 Section 3 and RFC 6125. Uppercase SNI hostnames must select
// the same context as their lowercase equivalents.

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
};

const SNIContexts = {
  'a.example.com': {
    key: loadPEM('agent1-key'),
    cert: loadPEM('agent1-cert'),
  },
  '*.test.com': {
    key: loadPEM('agent3-key'),
    cert: loadPEM('agent3-cert'),
  },
};

const tests = [
  // Exact match, uppercase
  { servername: 'A.EXAMPLE.COM', expectedCert: 'agent1', desc: 'uppercase exact' },
  // Mixed case
  { servername: 'A.Example.Com', expectedCert: 'agent1', desc: 'mixed case exact' },
  // Wildcard match, uppercase
  { servername: 'B.TEST.COM', expectedCert: 'agent3', desc: 'uppercase wildcard' },
  // Wildcard match, mixed case
  { servername: 'b.Test.Com', expectedCert: 'agent3', desc: 'mixed case wildcard' },
  // Lowercase still works
  { servername: 'a.example.com', expectedCert: 'agent1', desc: 'lowercase exact' },
];

let remaining = tests.length;

for (const { servername, expectedCert, desc } of tests) {
  const server = tls.createServer(serverOptions, common.mustCall((c) => {
    // The server should have selected the correct SNI context regardless of case
    const cert = c.getCertificate();
    assert.strictEqual(
      cert.subject.CN, expectedCert,
      `${desc}: expected server cert CN=${expectedCert}, got ${cert.subject.CN}`
    );
    c.end();
  }));

  server.addContext('a.example.com', SNIContexts['a.example.com']);
  server.addContext('*.test.com', SNIContexts['*.test.com']);

  server.listen(0, common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      servername,
      rejectUnauthorized: false,
    }, common.mustCall(() => {
      client.end();
    }));

    client.on('close', common.mustCall(() => {
      server.close();
      if (--remaining === 0) {
        process.stdout.write('all tests passed\n');
      }
    }));
  }));
}
