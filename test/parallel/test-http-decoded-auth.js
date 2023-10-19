'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const testCases = [
  {
    username: 'test@test"',
    password: '123456^',
    expected: 'dGVzdEB0ZXN0IjoxMjM0NTZe'
  },
  {
    username: 'test%40test',
    password: '123456',
    expected: 'dGVzdEB0ZXN0OjEyMzQ1Ng=='
  },
  {
    username: 'not%3Agood',
    password: 'god',
    expected: 'bm90Omdvb2Q6Z29k'
  },
  {
    username: 'not%22good',
    password: 'g%5Eod',
    expected: 'bm90Imdvb2Q6Z15vZA=='
  },
  {
    username: 'test1234::::',
    password: 'mypass',
    expected: 'dGVzdDEyMzQ6Ojo6Om15cGFzcw=='
  },
];

for (const testCase of testCases) {
  const server = http.createServer(function(request, response) {
    // The correct authorization header is be passed
    assert.strictEqual(request.headers.authorization, `Basic ${testCase.expected}`);
    response.writeHead(200, {});
    response.end('ok');
    server.close();
  });

  server.listen(0, function() {
    // make the request
    const url = new URL(`http://${testCase.username}:${testCase.password}@localhost:${this.address().port}`);
    http.request(url).end();
  });
}
