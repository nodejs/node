'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.createServer(common.mustCall((req, res) => {
  res.end('ok');
}));
server.listen(0, () => {
  http.get({
    port: server.address().port,
    headers: {'Test': 'Düsseldorf'}
  }, common.mustCall((res) => {
    assert.equal(res.statusCode, 200);
    server.close();
  }));
});
