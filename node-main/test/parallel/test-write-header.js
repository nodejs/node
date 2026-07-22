'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

common.expectWarning('DeprecationWarning',
                     'ServerResponse.prototype.writeHeader is deprecated.', 'DEP0063');

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHeader(200, [ 'test', '2', 'test2', '2' ]);
  res.end();
})).listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall((res) => {
    assert.strictEqual(res.headers.test, '2');
    assert.strictEqual(res.headers.test2, '2');
    res.resume().on('end', common.mustCall(() => {
      server.close();
    }));
  }));
}));
