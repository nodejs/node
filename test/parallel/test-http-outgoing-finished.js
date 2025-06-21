'use strict';
const common = require('../common');
const { finished } = require('stream');

const http = require('http');
const assert = require('assert');

const server = http.createServer(function(req, res) {
  let closed = false;
  res
    .on('close', common.mustCall(() => {
      closed = true;
      finished(res, common.mustCall(() => {
        server.close();
      }));
    }))
    .end();
  finished(res, common.mustCall(() => {
    assert.strictEqual(closed, true);
  }));

}).listen(0, function() {
  http
    .request({
      port: this.address().port,
      method: 'GET'
    })
    .on('response', function(res) {
      res.resume();
    })
    .end();
});
