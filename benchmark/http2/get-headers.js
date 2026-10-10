'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const http2 = require('node:http2');

const bench = common.createBenchmark(main, {
  n: [1e6],
  headers: [0, 1, 4, 16, 64],
});

function main({ n, headers }) {
  const server = http2.createServer();
  server.on('request', (request, response) => {
    const expected = { __proto__: null };
    for (let index = 0; index < headers; index++) {
      const name = `x-header-${index}`;
      response.setHeader(name, 'value');
      expected[name] = 'value';
    }

    for (let index = 0; index < 1e4; index++) {
      response.getHeaders();
    }

    let result;
    bench.start();
    for (let index = 0; index < n; index++) {
      result = response.getHeaders();
    }
    bench.end(n);
    assert.deepStrictEqual(result, expected);
    response.end();
  });

  server.listen(0, '127.0.0.1', () => {
    const client = http2.connect(`http://127.0.0.1:${server.address().port}`);
    const request = client.request();
    request.resume();
    request.on('end', () => {
      client.close();
      server.close();
    });
    request.end();
  });
}
