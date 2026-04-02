'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Test 1: req.signal is an AbortSignal and aborts on 'close'
{
  const server = http.createServer(common.mustCall((req, res) => {
    assert.ok(req.signal instanceof AbortSignal);
    assert.strictEqual(req.signal.aborted, false);
    req.signal.onabort = common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, true);
    });
    res.destroy();
  }));
  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, () => {}).on('error', () => {
      server.close();
    });
  }));
}

// Test 2: req.signal is aborted if accessed after destroy
{
  const req = new http.IncomingMessage(null);
  req.destroy();
  assert.strictEqual(req.signal.aborted, true);
}

// Test 3: Multiple accesses return the same signal
{
  const req = new http.IncomingMessage(null);
  assert.strictEqual(req.signal, req.signal);
}

// Test 4: req.signal aborts when 'abort' event is emitted on req
{
  const req = new http.IncomingMessage(null);
  const signal = req.signal;
  assert.strictEqual(signal.aborted, false);
  req.emit('abort');
  assert.strictEqual(signal.aborted, true);
}

// Test 5: res.signal on a client-side http.request() response (IncomingMessage).
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200);
    res.write('partial');
  }));

  server.listen(0, common.mustCall(() => {
    const clientReq = http.request(
      { port: server.address().port },
      common.mustCall((res) => {
        assert.ok(res.signal instanceof AbortSignal);
        assert.strictEqual(res.signal.aborted, false);

        res.signal.onabort = common.mustCall(() => {
          assert.strictEqual(res.signal.aborted, true);
          server.close();
        });
        clientReq.destroy();
      }),
    );
    clientReq.on('error', () => {});
    clientReq.end();
  }));
}
