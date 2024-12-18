'use strict';
const common = require('../common.js');
const fixtures = require('../../test/common/fixtures');
const tls = require('tls');

const bench = common.createBenchmark(main, {
  n: [1, 5],
  ca: [0, 1],
});

function main(conf) {
  const n = conf.n;

  const options = {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
    ca: conf.ca === 1 ? [fixtures.readKey('rsa_ca.crt')] : undefined,
  };

  bench.start();
  tls.createSecureContext(options);
  bench.end(n);
}
