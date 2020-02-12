'use strict';

const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  n: [10, 1000],
  len: [1, 100],
  duration: 5
});

function main({ len, n, duration }) {
  const headers = {
    'Connection': 'keep-alive',
    'Transfer-Encoding': 'chunked',
  };

  // TODO(BridgeAR): Change this benchmark to use grouped arguments when
  // implemented. https://github.com/nodejs/node/issues/26425
  const Is = [ ...Array(Math.max(n / len, 1)).keys() ];
  const Js = [ ...Array(len).keys() ];
  for (const i of Is) {
    headers[`foo${i}`] = Js.map(() => `some header value ${i}`);
  }

  const server = http.createServer((req, res) => {
    res.writeHead(200, headers);
    res.end();
  });
  server.listen(common.PORT, () => {
    bench.http({
      path: '/',
      connections: 10,
      duration
    }, () => {
      server.close();
    });
  });
}
