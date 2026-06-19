'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// 1) Fires once, before serialization, for an implicit-header response (end()).
{
  const server = http.createServer(common.mustCall((req, res) => {
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
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers['x-added-in-hook'], 'yes');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 2) Listener may change the status code, and it is reflected on the wire.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503;
      this.statusMessage = 'Service Unavailable';
    }));
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 503);
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 3) Listener sees and can remove headers passed INLINE to writeHead(),
//    including the array form.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      assert.strictEqual(this.getHeader('X-Inline'), 'a');
      this.removeHeader('X-Inline');
      this.setHeader('X-From-Hook', 'b');
    }));
    res.writeHead(200, ['X-Inline', 'a', 'Content-Type', 'text/plain']);
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers['x-inline'], undefined);
      assert.strictEqual(res.headers['x-from-hook'], 'b');
      assert.strictEqual(res.headers['content-type'], 'text/plain');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 4) Multiple listeners all run (FIFO, EventEmitter semantics).
{
  const order = [];
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(() => order.push(1)));
    res.on('sendingHeaders', common.mustCall(() => order.push(2)));
    res.end();
    assert.deepStrictEqual(order, [1, 2]);
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 5) flushHeaders() also triggers the hook exactly once.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.setHeader('X-Flushed', '1');
    }));
    res.flushHeaders();
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers['x-flushed'], '1');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 6) The hook is synchronous: only changes made before an `await` take effect.
//    Work deferred past `await` runs after the headers are sent, so a later
//    setHeader() throws and a statusCode change is not reflected on the wire.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(async function() {
      this.setHeader('X-Sync', 'before');
      await null;
      assert.strictEqual(this.headersSent, true);
      this.statusCode = 503; // No effect: headers already sent.
      assert.throws(() => this.setHeader('X-Async', 'after'), {
        code: 'ERR_HTTP_HEADERS_SENT',
      });
      server.close();
    }));
    res.end('hello');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.headers['x-sync'], 'before');
      assert.strictEqual(res.headers['x-async'], undefined);
      res.resume();
    }));
  }));
}
