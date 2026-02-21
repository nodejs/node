'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const { getEventListeners } = require('events');

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

  const server = http.createServer(common.mustNotCall());

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


{
  // Destroy post-abort sync with AbortSignal

  const server = http.createServer(common.mustNotCall());
  const controller = new AbortController();
  const { signal } = controller;
  server.listen(0, common.mustCall(() => {
    const options = { port: server.address().port, signal };
    const req = http.get(options, common.mustNotCall());
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ABORT_ERR');
      assert.strictEqual(err.name, 'AbortError');
      server.close();
    }));
    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, false);
    controller.abort();
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, true);
  }));
}

{
  // Use post-abort async AbortSignal
  const server = http.createServer(common.mustNotCall());
  const controller = new AbortController();
  const { signal } = controller;
  server.listen(0, common.mustCall(() => {
    const options = { port: server.address().port, signal };
    const req = http.get(options, common.mustNotCall());
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ABORT_ERR');
      assert.strictEqual(err.name, 'AbortError');
    }));

    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.aborted, false);
      assert.strictEqual(req.destroyed, true);
      server.close();
    }));

    assert.strictEqual(getEventListeners(signal, 'abort').length, 1);
    process.nextTick(() => controller.abort());
  }));
}

{
  // Use pre-aborted AbortSignal
  const server = http.createServer(common.mustNotCall());
  const controller = new AbortController();
  const { signal } = controller;
  server.listen(0, common.mustCall(() => {
    controller.abort();
    const options = { port: server.address().port, signal };
    const req = http.get(options, common.mustNotCall());
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ABORT_ERR');
      assert.strictEqual(err.name, 'AbortError');
      server.close();
    }));
    assert.strictEqual(req.aborted, false);
    assert.strictEqual(req.destroyed, true);
  }));
}
