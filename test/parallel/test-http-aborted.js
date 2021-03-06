'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

{
  const server = http.createServer(common.mustCall(function(req, res) {
    req.on('aborted', common.mustCall(function() {
      assert.strictEqual(this.aborted, true);
    }));
    req.on('error', common.mustCall(function(err) {
      assert.strictEqual(err.code, 'ECONNRESET');
      assert.strictEqual(err.message, 'aborted');
      server.close();
    }));
    assert.strictEqual(req.aborted, false);
    res.write('hello');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.get({
      port: server.address().port,
      headers: { connection: 'keep-alive' }
    }, common.mustCall((res) => {
      res.on('aborted', common.mustCall(() => {
        assert.strictEqual(res.aborted, true);
      }));
      res.on('error', common.expectsError({
        code: 'ECONNRESET',
        message: 'aborted'
      }));
      req.abort();
    }));
  }));
}

{
  // Don't crash if no 'error' handler on server request.

  const server = http.createServer(common.mustCall(function(req, res) {
    req.on('aborted', common.mustCall(function() {
      assert.strictEqual(this.aborted, true);
      server.close();
    }));
    assert.strictEqual(req.aborted, false);
    res.write('hello');
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.get({
      port: server.address().port,
      headers: { connection: 'keep-alive' }
    }, common.mustCall((res) => {
      res.on('aborted', common.mustCall(() => {
        assert.strictEqual(res.aborted, true);
      }));
      req.abort();
    }));
  }));
}
