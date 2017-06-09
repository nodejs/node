'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  assert.doesNotThrow(() => {
    res.setHeader('header1', 1);
  });
  res.write('abc');
  assert.throws(() => {
    res.setHeader('header2', 2);
  }, /Can't set headers after they are sent\./);
  res.end();
});

server.listen(0, () => {
  http.get({port: server.address().port}, () => {
    server.close();
  });
});
