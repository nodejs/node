'use strict';

// Benchmarks the fetch() handler of the experimental Fetch Server API
// (http.fetchServer) over HTTP/2, with prior knowledge (h2c) and over TLS.

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  tls: [0, 1],
  len: [4, 1024, 102400],
  requests: [100, 1000, 5000],
  streams: [1, 10, 100],
  clients: [2],
  benchmarker: ['test-double-http2'],
  duration: 5,
}, { flags: ['--experimental-http-serve', '--no-warnings'] });

function main({ tls, len, requests, streams, clients, duration }) {
  const { serve, ServerResponse } = require('http').fetchServer;
  const body = Buffer.alloc(len, 'x');
  const options = { port: 0, hostname: '127.0.0.1' };
  if (tls) {
    const fixtures = require('../../test/common/fixtures');
    options.tls = {
      key: fixtures.readKey('rsa_private.pem'),
      cert: fixtures.readKey('rsa_cert.crt'),
      alpn: ['h2'],
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
    bench.http({
      path: '/',
      port: listener.address.port,
      scheme: tls ? 'https' : 'http',
      requests,
      maxConcurrentStreams: streams,
      clients,
      threads: clients,
      duration,
    }, () => {
      server.destroy();
    });
  });
}
