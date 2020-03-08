'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

{
  // abort

  const server = http.createServer(common.mustCall((req, res) => {
    res.end('Hello');
  }));

  server.listen(0, common.mustCall(() => {
    const options = { port: server.address().port };
    const req = http.get(options, common.mustCall((res) => {
      res.on('data', (data) => {
        req.abort();
        assert.strictEqual(req.aborted, true);
        assert.strictEqual(req.destroyed, true);
        server.close();
      });
    }));
    req.on('error', common.mustNotCall());
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, false);
  }));
}

{
  // destroy + res

  const server = http.createServer(common.mustCall((req, res) => {
    res.end('Hello');
  }));

  server.listen(0, common.mustCall(() => {
    const options = { port: server.address().port };
    const req = http.get(options, common.mustCall((res) => {
      res.on('data', (data) => {
        req.destroy();
        assert.strictEqual(req.aborted, false);
        assert.strictEqual(req.destroyed, true);
        server.close();
      });
    }));
    req.on('error', common.mustNotCall());
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, false);
  }));
}

{
  // destroy

  const server = http.createServer(common.mustNotCall((req, res) => {
  }));

  server.listen(0, common.mustCall(() => {
    const options = { port: server.address().port };
    const req = http.get(options, common.mustNotCall());
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
      server.close();
    }));
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, false);
    req.destroy();
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, true);
  }));
}
