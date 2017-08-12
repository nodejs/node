'use strict';

const common = require('../common.js');
const PORT = common.PORT;

var bench = common.createBenchmark(main, {
  n: [1e3],
  nheaders: [0, 10, 100, 1000],
}, { flags: ['--expose-http2', '--no-warnings'] });

function main(conf) {
  const n = +conf.n;
  const nheaders = +conf.nheaders;
  const http2 = require('http2');
  const server = http2.createServer();

  const headersObject = {
    ':path': '/',
    ':scheme': 'http',
    'accept-encoding': 'gzip, deflate',
    'accept-language': 'en',
    'content-type': 'text/plain',
    'referer': 'https://example.org/',
    'user-agent': 'SuperBenchmarker 3000'
  };

  for (var i = 0; i < nheaders; i++) {
    headersObject[`foo${i}`] = `some header value ${i}`;
  }

  server.on('stream', (stream) => {
    stream.respond();
    stream.end('Hi!');
  });
  server.listen(PORT, () => {
    const client = http2.connect(`http://localhost:${PORT}/`);

    function doRequest(remaining) {
      const req = client.request(headersObject);
      req.end();
      req.on('data', () => {});
      req.on('end', () => {
        if (remaining > 0) {
          doRequest(remaining - 1);
        } else {
          bench.end(n);
          server.close();
          client.destroy();
        }
      });
    }

    bench.start();
    doRequest(n);
  });
}
