'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.Server(common.mustCall(function(req, res) {
  res.on('error', common.mustCall(function onResError(err) {
    assert.strictEqual(err.message, 'write after end');
  }));

  res.write('This should write.');
  res.end();

  const r = res.write('This should raise an error.');
  assert.strictEqual(r, true, 'write after end should return true');
}));

server.listen(0, function() {
  http.get({port: this.address().port}, function(res) {
    server.close();
  });
});
