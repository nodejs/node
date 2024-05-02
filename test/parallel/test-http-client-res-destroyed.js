'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('asd');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port
    }, common.mustCall((res) => {
      assert.strictEqual(res.destroyed, false);
      res.destroy();
      assert.strictEqual(res.destroyed, true);
      res.on('close', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}

{
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('asd');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port
    }, common.mustCall((res) => {
      assert.strictEqual(res.destroyed, false);
      res.on('close', common.mustCall(() => {
        assert.strictEqual(res.destroyed, true);
        server.close();
      })).resume();
    }));
  }));
}
