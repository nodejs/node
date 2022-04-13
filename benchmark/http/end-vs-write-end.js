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
  type: ['asc', 'utf', 'buf'],
  len: [64 * 1024, 128 * 1024, 256 * 1024, 1024 * 1024],
  c: [100],
  method: ['write', 'end'],
  duration: 5
});

function main({ len, type, method, c, duration }) {
  const http = require('http');
  let chunk;
  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(len, 'x');
      break;
    case 'utf':
      chunk = 'ü'.repeat(len / 2);
      break;
    case 'asc':
      chunk = 'a'.repeat(len);
      break;
  }

  function write(res) {
    res.write(chunk);
    res.end();
  }

  function end(res) {
    res.end(chunk);
  }

  const fn = method === 'write' ? write : end;

  const server = http.createServer((req, res) => {
    fn(res);
  });

  server.listen(0, () => {
    bench.http({
      connections: c,
      duration,
      port: server.address().port,
    }, () => {
      server.close();
    });
  });
}
