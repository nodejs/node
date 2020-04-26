'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const httpServer = http.createServer(common.mustCall(function(req, res) {
  httpServer.close();
  assert.throws(() => {
    res.end(['Throws.']);
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  res.end();
}));

httpServer.listen(0, common.mustCall(function() {
  http.get({ port: this.address().port });
}));
