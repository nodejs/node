'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const expectedSuccesses = [undefined, null, 'GET', 'post'];
let requestCount = 0;

const server = http.createServer((req, res) => {
  requestCount++;
  res.end();

  if (expectedSuccesses.length === requestCount) {
    server.close();
  }
}).listen(0, test);

function test() {
  function fail(input) {
    assert.throws(() => {
      http.request({ method: input, path: '/' }, common.fail);
    }, /^TypeError: Method must be a string$/);
  }

  fail(-1);
  fail(1);
  fail(0);
  fail({});
  fail(true);
  fail(false);
  fail([]);

  function ok(method) {
    http.request({ method: method, port: server.address().port }).end();
  }

  expectedSuccesses.forEach((method) => {
    ok(method);
  });
}
