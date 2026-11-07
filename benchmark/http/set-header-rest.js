'use strict';

const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  headers: [2, 12],
  values: ['static', 'dynamic'],
  duration: 5,
});

const restHeaders = [
  ['Content-Type', 'application/json; charset=utf-8'],
  ['Cache-Control', 'no-store'],
  ['X-Content-Type-Options', 'nosniff'],
  ['X-Frame-Options', 'DENY'],
  ['Strict-Transport-Security', 'max-age=31536000; includeSubDomains'],
  ['Access-Control-Allow-Origin', '*'],
  ['Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE'],
  ['Vary', 'Origin, Accept-Encoding'],
  ['X-RateLimit-Limit', '1000'],
  ['X-RateLimit-Remaining', '999'],
  ['ETag', 'W/"2a-abc"'],
  ['X-Request-Id', '00000000'],
];

function main({ headers, values, duration }) {
  const list = restHeaders.slice(0, headers);
  const body = '{"ok":true}';
  let id = 0;

  const server = http.createServer((req, res) => {
    for (const { 0: name, 1: value } of list) {
      res.setHeader(name, values === 'dynamic' ? `${value}-${id}` : value);
    }
    id++;
    res.end(body);
  });

  server.listen(0, () => {
    bench.http({
      path: '/',
      connections: 100,
      duration,
      port: server.address().port,
    }, () => {
      server.close();
    });
  });
}
