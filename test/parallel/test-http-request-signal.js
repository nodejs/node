'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Test 1: req.signal is an AbortSignal and aborts on socket close
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

// Test 2: req.signal is not aborted when a request body completes normally.
{
  const body = JSON.stringify({ hello: 'world' });
  const server = http.createServer(common.mustCall((req, res) => {
    assert.ok(req.signal instanceof AbortSignal);
    assert.strictEqual(req.signal.aborted, false);
    req.signal.onabort = common.mustNotCall();

    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.aborted, false);
      assert.strictEqual(req.complete, true);
      assert.strictEqual(req.signal.aborted, false);
    }));

    req.on('end', common.mustCall(() => {
      setTimeout(common.mustCall(() => {
        assert.strictEqual(req.aborted, false);
        assert.strictEqual(req.complete, true);
        assert.strictEqual(req.signal.aborted, false);
        res.end('ok');
      }), 10);
    }));
    req.resume();
  }));

  server.listen(0, common.mustCall(() => {
    const clientReq = http.request(
      {
        port: server.address().port,
        method: 'PATCH',
        path: '/tables/1',
        headers: {
          'content-type': 'application/json',
          'content-length': Buffer.byteLength(body),
        },
      },
      common.mustCall((res) => {
        res.resume();
        res.on('end', common.mustCall(() => {
          server.close();
        }));
      }),
    );
    clientReq.end(body);
  }));
}

// Test 3: req.signal is aborted if accessed after destroy
{
  const req = new http.IncomingMessage(null);
  req.destroy();
  assert.strictEqual(req.signal.aborted, true);
}

// Test 4: Multiple accesses return the same signal
{
  const req = new http.IncomingMessage(null);
  assert.strictEqual(req.signal, req.signal);
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

// Test 6: res.signal is not aborted when a response body completes normally.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const clientReq = http.request(
      { port: server.address().port },
      common.mustCall((res) => {
        assert.ok(res.signal instanceof AbortSignal);
        assert.strictEqual(res.signal.aborted, false);
        res.signal.onabort = common.mustNotCall();

        res.resume();
        res.on('end', common.mustCall(() => {
          assert.strictEqual(res.complete, true);
          assert.strictEqual(res.signal.aborted, false);
        }));
        res.on('close', common.mustCall(() => {
          assert.strictEqual(res.signal.aborted, false);
          server.close();
        }));
      }),
    );
    clientReq.end();
  }));
}

// Test 7: Client cancels a pending request.
{
  const server = http.createServer(common.mustCall((req, res) => {
    req.signal.onabort = common.mustCall(() => {
      assert.strictEqual(req.signal.aborted, true);
      server.close();
    });
    res.flushHeaders();
  }));

  server.listen(0, common.mustCall(() => {
    const clientReq = http.request(
      { port: server.address().port },
      common.mustCall((res) => {
        res.on('error', () => {});
        clientReq.destroy();
      }),
    );
    clientReq.on('error', () => {});
    clientReq.end();
  }));
}
