// When calling .end(buffer) right away, this triggers a "hot path"
// optimization in http.js, to avoid an extra write call.
//
// However, the overhead of copying a large buffer is higher than
// the overhead of an extra write() call, so the hot path was not
// always as hot as it could be.
//
// Verify that our assumptions are valid.
'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1, 4, 8, 16],
  len: [1, 64, 256],
  c: [100],
  duration: 5
});

function main({ len, n, c, duration }) {
  const http = require('http');
  const chunk = Buffer.alloc(len, '8');

  const server = http.createServer((req, res) => {
    function send(left) {
      if (left === 0) return res.end();
      res.write(chunk);
      setTimeout(() => {
        send(left - 1);
      }, 0);
    }
    send(n);
  });

  server.listen(common.PORT, () => {
    bench.http({
      connections: c,
      duration
    }, () => {
      server.close();
    });
  });
}
