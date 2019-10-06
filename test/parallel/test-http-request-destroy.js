'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  const server = http.Server(function(req, res) {
    req.destroy();
    assert.strictEqual(req.destroyed, true);
  });

  server.listen(0, function() {
    http.get({
      port: this.address().port,
    }).on('error', common.mustCall(() => {
      server.close();
    }));
  });
}

{
  const server = http.Server(function(req, res) {
    req.destroy(new Error('kaboom'));
    req.destroy(new Error('kaboom2'));
    assert.strictEqual(req.destroyed, true);
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.message, 'kaboom');
    }));
  });

  server.listen(0, function() {
    http.get({
      port: this.address().port,
    }).on('error', common.mustCall(() => {
      server.close();
    }));
  });
}
