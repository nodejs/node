'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// The 'sendingHeaders' event on the HTTP/2 compatibility ServerResponse mirrors
// the one on the HTTP/1 ServerResponse: it is emitted synchronously, exactly
// once, immediately before the HEADERS frame is sent, and a listener may still
// mutate headers and the status code.

// 1) Fires once, before serialization, for an implicit-header response (end()).
{
  const server = http2.createServer(common.mustCall((req, res) => {
    let fired = 0;
    res.on('sendingHeaders', common.mustCall(function() {
      fired++;
      // Headers not yet sent, still mutable.
      assert.strictEqual(this.headersSent, false);
      this.setHeader('X-Added-In-Hook', 'yes');
    }));
    res.end('body');
    // Synchronous: hook has already run by the time end() returns.
    assert.strictEqual(fired, 1);
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers['x-added-in-hook'], 'yes');
    }));
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end();
  }));
}

// 2) Listener may change the status code, and it is reflected on the wire.
{
  const server = http2.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503;
    }));
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 503);
    }));
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end();
  }));
}

// 3) Listener sees and can remove headers passed INLINE to writeHead(),
//    including the array form.
{
  const server = http2.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      assert.strictEqual(this.getHeader('X-Inline'), 'a');
      this.removeHeader('X-Inline');
      this.setHeader('X-From-Hook', 'b');
    }));
    res.writeHead(200, ['X-Inline', 'a', 'Content-Type', 'text/plain']);
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers['x-inline'], undefined);
      assert.strictEqual(headers['x-from-hook'], 'b');
      assert.strictEqual(headers['content-type'], 'text/plain');
    }));
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end();
  }));
}

// 4) Multiple listeners all run (FIFO, EventEmitter semantics).
{
  const order = [];
  const server = http2.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(() => order.push(1)));
    res.on('sendingHeaders', common.mustCall(() => order.push(2)));
    res.end();
    assert.deepStrictEqual(order, [1, 2]);
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end();
  }));
}

// 5) flushHeaders() also triggers the hook exactly once.
{
  const server = http2.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.setHeader('X-Flushed', '1');
    }));
    res.flushHeaders();
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers['x-flushed'], '1');
    }));
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end();
  }));
}

// 6) The hook is synchronous: only changes made before an `await` take effect.
//    Work deferred past `await` runs after the headers are sent, so a later
//    setHeader() throws and a statusCode change is not reflected on the wire.
{
  const server = http2.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(async function() {
      this.setHeader('X-Sync', 'before');
      await null;
      assert.strictEqual(this.headersSent, true);
      this.statusCode = 503; // No effect: headers already sent.
      assert.throws(() => this.setHeader('X-Async', 'after'), {
        code: 'ERR_HTTP2_HEADERS_SENT',
      });
      server.close();
    }));
    res.end('hello');
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      assert.strictEqual(headers['x-sync'], 'before');
      assert.strictEqual(headers['x-async'], undefined);
    }));
    req.resume();
    req.on('end', common.mustCall(() => client.close()));
    req.end();
  }));
}

// Re-entrancy: a listener that flushes the headers itself re-enters writeHead().
// The emit is guarded so the event fires exactly once (common.mustCall), and the
// illegal self-flush surfaces as ERR_HTTP2_HEADERS_SENT rather than recursing.
{
  const server = http2.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.flushHeaders();
    }));
    assert.throws(() => res.end('hello'), { code: 'ERR_HTTP2_HEADERS_SENT' });
    res.destroy();
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('error', () => {}); // the stream reset from destroy() is expected
    req.on('close', () => client.close());
    req.end();
  }));
}
