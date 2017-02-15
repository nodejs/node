'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  assert.doesNotThrow(() => {
    res.removeHeader('header1', 1);
  });
  res.write('abc');
  assert.throws(() => {
    res.removeHeader('header2', 2);
  }, /Can't remove headers after they are sent/);
  res.end();
});

server.listen(0, () => {
  http.get({port: server.address().port}, () => {
    server.close();
  });
});
