'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const expectedHeaders = {
  'DELETE': ['host', 'connection'],
  'GET': ['host', 'connection'],
  'HEAD': ['host', 'connection'],
  'OPTIONS': ['host', 'connection'],
  'POST': ['host', 'connection', 'content-length'],
  'PUT': ['host', 'connection', 'content-length']
};

const expectedMethods = Object.keys(expectedHeaders);

const countdown =
  new Countdown(expectedMethods.length,
                common.mustCall(() => server.close()));

const server = http.createServer(common.mustCall((req, res) => {
  res.end();

  assert(expectedHeaders.hasOwnProperty(req.method),
         `${req.method} was an unexpected method`);

  const requestHeaders = Object.keys(req.headers);
  requestHeaders.forEach((header) => {
    assert.strictEqual(
      expectedHeaders[req.method].includes(header.toLowerCase()),
      true,
      `${header} should not exist for method ${req.method}`
    );
  });

  assert.strictEqual(
    requestHeaders.length,
    expectedHeaders[req.method].length,
    `some headers were missing for method: ${req.method}`
  );

  countdown.dec();
}, expectedMethods.length));

server.listen(0, common.mustCall(() => {
  expectedMethods.forEach((method) => {
    http.request({
      method: method,
      port: server.address().port
    }).end();
  });
}));
