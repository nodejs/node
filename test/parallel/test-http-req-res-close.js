'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

// When the response is ended immediately, `req` should emit `close`
// after `res`
{
  const server = http.Server(common.mustCall((req, res) => {
    let resClosed = false;
    let reqClosed = false;

    res.end();
    let resFinished = false;
    res.on('finish', common.mustCall(() => {
      resFinished = true;
      assert.strictEqual(resClosed, false);
      assert.strictEqual(res.destroyed, false);
      assert.strictEqual(resClosed, false);
    }));
    assert.strictEqual(req.destroyed, false);
    res.on('close', common.mustCall(() => {
      resClosed = true;
      assert.strictEqual(resFinished, true);
      assert.strictEqual(reqClosed, false);
      assert.strictEqual(res.destroyed, true);
    }));
    assert.strictEqual(req.destroyed, false);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(req.destroyed, false);
    }));
    req.on('close', common.mustCall(() => {
      reqClosed = true;
      assert.strictEqual(resClosed, true);
      assert.strictEqual(req.destroyed, true);
      assert.strictEqual(req._readableState.ended, true);
    }));
    res.socket.on('close', () => server.close());
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall());
  }));
}

// When there's no `data` handler attached to `req`,
// `req` should emit `close` after `res`.
{
  const server = http.Server(common.mustCall((req, res) => {
    let resClosed = false;
    let reqClosed = false;

    // This time, don't end the response immediately
    setTimeout(() => res.end(), 100);
    let resFinished = false;
    res.on('finish', common.mustCall(() => {
      resFinished = true;
      assert.strictEqual(resClosed, false);
      assert.strictEqual(res.destroyed, false);
      assert.strictEqual(resClosed, false);
    }));
    assert.strictEqual(req.destroyed, false);
    res.on('close', common.mustCall(() => {
      resClosed = true;
      assert.strictEqual(resFinished, true);
      assert.strictEqual(reqClosed, false);
      assert.strictEqual(res.destroyed, true);
    }));
    assert.strictEqual(req.destroyed, false);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(req.destroyed, false);
    }));
    req.on('close', common.mustCall(() => {
      reqClosed = true;
      assert.strictEqual(resClosed, true);
      assert.strictEqual(req.destroyed, true);
      assert.strictEqual(req._readableState.ended, true);
    }));
    res.socket.on('close', () => server.close());
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall());
  }));
}

// When a `data` handler is `attached` to `req`
// (i.e. the server is consuming  data from it), `req` should emit `close`
// before `res`.
// https://github.com/nodejs/node/pull/33035 introduced this change in behavior.
// See https://github.com/nodejs/node/pull/33035#issuecomment-751482764
{
  const server = http.Server(common.mustCall((req, res) => {
    let resClosed = false;
    let reqClosed = false;

    // Don't end the response immediately
    setTimeout(() => res.end(), 100);
    let resFinished = false;
    req.on('data', () => {});
    res.on('finish', common.mustCall(() => {
      resFinished = true;
      assert.strictEqual(resClosed, false);
      assert.strictEqual(res.destroyed, false);
      assert.strictEqual(resClosed, false);
    }));
    assert.strictEqual(req.destroyed, false);
    res.on('close', common.mustCall(() => {
      resClosed = true;
      assert.strictEqual(resFinished, true);
      assert.strictEqual(reqClosed, true);
      assert.strictEqual(res.destroyed, true);
    }));
    assert.strictEqual(req.destroyed, false);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(req.destroyed, false);
    }));
    req.on('close', common.mustCall(() => {
      reqClosed = true;
      assert.strictEqual(resClosed, false);
      assert.strictEqual(req.destroyed, true);
      assert.strictEqual(req._readableState.ended, true);
    }));
    res.socket.on('close', () => server.close());
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall());
  }));
}
