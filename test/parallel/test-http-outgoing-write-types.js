'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const httpServer = http.createServer(common.mustCall(function(req, res) {
  httpServer.close();
  assert.throws(() => {
    res.write(['Throws.']);
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  // should not throw
  res.write('1a2b3c');
  // should not throw
  res.write(new Uint8Array(1024));
  // should not throw
  res.write(Buffer.from('1'.repeat(1024)));
  res.end();
}));

httpServer.listen(0, common.mustCall(function() {
  http.get({ port: this.address().port });
}));
