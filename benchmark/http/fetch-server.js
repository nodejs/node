'use strict';

// Benchmarks the fetch() handler of the experimental Fetch Server API
// (http.fetchServer) over HTTP/1.1, with and without TLS.

const common = require('../common.js');
const { default_http_benchmarker } = require('../_http-benchmarkers.js');

const bench = common.createBenchmark(main, {
  tls: [0, 1],
  len: [4, 1024, 102400],
  c: [50, 500],
  duration: 5,
}, { flags: ['--experimental-http-serve', '--no-warnings'] });

function main({ tls, len, c, duration }) {
  const { serve, ServerResponse } = require('http').fetchServer;
  const body = Buffer.alloc(len, 'x');
  const options = { port: 0, hostname: '127.0.0.1' };
  if (tls) {
    const fixtures = require('../../test/common/fixtures');
    options.tls = {
      key: fixtures.readKey('rsa_private.pem'),
      cert: fixtures.readKey('rsa_cert.crt'),
    };
  }
  const server = serve({
    fetch() {
      return new ServerResponse(body, {
        headers: { 'content-type': 'text/plain' },
      });
    },
  }, options);

  server.ready.then(() => {
    const [listener] = server;
    // The test double for plain HTTP cannot speak TLS.
    const benchmarker = tls && default_http_benchmarker === 'test-double-http' ?
      'test-double-https' : undefined;
    bench.http({
      path: '/',
      port: listener.address.port,
      scheme: tls ? 'https' : 'http',
      connections: c,
      duration,
      benchmarker,
    }, () => {
      server.destroy();
    });
  });
}
