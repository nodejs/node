'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  assert.doesNotThrow(() => {
    res.removeHeader('header1', 1);
  });
  res.write('abc')
  assert.throws(() => {
    res.removeHeader('header2', 2);
  }, Error, 'removeHeader after write should throw');
  res.end()
});

server.listen(common.PORT, () => {
  http.get({port: server.address().port}, () => {
    server.close()
  });
});
