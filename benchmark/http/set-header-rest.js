'use strict';

const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  headers: [2, 12],
  values: ['static', 'dynamic'],
  // 'many' rotates through more unique names than the validation cache holds.
  names: ['fixed', 'many'],
  // Also set headers carrying credentials, whose values are never cached.
  credentials: ['none', 'static', 'dynamic'],
  duration: 5,
});

const credentialHeaders = [
  ['Authorization', 'Bearer eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.c2lnbmF0dXJl'],
  ['Cookie', 'sid=0123456789abcdef0123456789abcdef; theme=dark'],
  ['Set-Cookie', 'sid=0123456789abcdef0123456789abcdef; Path=/; HttpOnly; Secure'],
];

const manyNames = [];
for (let i = 0; i < 1024; i++) {
  manyNames.push(`X-Custom-Header-${i}`);
}

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

function main({ headers, values, names, credentials, duration }) {
  const list = restHeaders.slice(0, headers);
  const body = '{"ok":true}';
  let id = 0;
  let next = 0;

  const server = http.createServer((req, res) => {
    for (const { 0: name, 1: value } of list) {
      res.setHeader(
        names === 'many' ? manyNames[next++ % manyNames.length] : name,
        values === 'dynamic' ? `${value}-${id}` : value,
      );
    }
    if (credentials !== 'none') {
      for (const { 0: name, 1: value } of credentialHeaders) {
        res.setHeader(name, credentials === 'dynamic' ? `${value}-${id}` : value);
      }
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
