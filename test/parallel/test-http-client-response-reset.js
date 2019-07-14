'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

let serverRes;
const server = http.Server(function(req, res) {
  res.write('Part of my res.');
  serverRes = res;
});

server.listen(0, common.mustCall(function() {
  http.get({
    port: this.address().port,
    headers: { connection: 'keep-alive' }
  }, common.mustCall(function(res) {
    server.close();
    serverRes.destroy();
    res.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
    }));
  }));
}));
