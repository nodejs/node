'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const expectedSuccesses = [undefined, null, 'GET', 'post'];
const expectedFails = [-1, 1, 0, {}, true, false, [], Symbol()];

const countdown =
  new Countdown(expectedSuccesses.length,
                common.mustCall(() => server.close()));

const server = http.createServer(common.mustCall((req, res) => {
  res.end();
  countdown.dec();
}, expectedSuccesses.length));

server.listen(0, common.mustCall(() => {
  expectedFails.forEach((method) => {
    assert.throws(() => {
      http.request({ method, path: '/' }, common.mustNotCall());
    }, /^TypeError: Method must be a string$/);
  });

  expectedSuccesses.forEach((method) => {
    http.request({ method, port: server.address().port }).end();
  });
}));
