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

// 7) A listener that flushes the headers itself re-enters writeHead(). The emit
//    is guarded so the event still fires exactly once (common.mustCall below),
//    and the illegal self-flush surfaces as ERR_HTTP_HEADERS_SENT rather than a
//    stack overflow.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.flushHeaders();
    }));
    assert.throws(() => res.end('hello'), { code: 'ERR_HTTP_HEADERS_SENT' });
    res.destroy();
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.get({ port: server.address().port });
    req.on('response', (res) => res.resume());
    req.on('error', () => {}); // The reset from destroy() is expected
  }));
}

// 8) A listener that changes only the status code gets the reason phrase that
//    matches the final code (not the one defaulted from the original code).
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503; // statusMessage intentionally left unset
    }));
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 503);
      assert.strictEqual(res.statusMessage, 'Service Unavailable');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 9) An explicit empty reason phrase is preserved (the deferred status-line
//    default must not overwrite it), and the listener sees the empty phrase.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      assert.strictEqual(this.statusMessage, '');
    }));
    res.writeHead(200, '');
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      assert.strictEqual(res.statusMessage, '');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 10) A listener may set a reason phrase that happens to equal a status-code
//     default while changing the code; the explicit choice is kept (the derived
//     phrase is only applied when none was set, detected via `=== undefined`).
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503;
      this.statusMessage = 'OK'; // Explicit, equals the 200 default
    }));
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 503);
      assert.strictEqual(res.statusMessage, 'OK');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 11) A listener that sets its own reason phrase keeps it, even when it also
//     changes the status code (the refresh only touches the auto-derived one).
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503;
      this.statusMessage = 'Custom Reason';
    }));
    res.writeHead(200);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 503);
      assert.strictEqual(res.statusMessage, 'Custom Reason');
      res.resume().on('end', common.mustCall(() => server.close()));
    }));
  }));
}

// 12) A listener that changes the status code and flushes the headers itself
//     re-enters writeHead(); the reason phrase is derived on that path too, so
//     it matches the final code (not a stale one). The illegal self-flush still
//     throws ERR_HTTP_HEADERS_SENT.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.on('sendingHeaders', common.mustCall(function() {
      this.statusCode = 503;
      this.flushHeaders();
    }));
    assert.throws(() => res.end('x'), { code: 'ERR_HTTP_HEADERS_SENT' });
    // The re-entrant writeHead() derived the reason phrase for the final code.
    assert.strictEqual(res.statusMessage, 'Service Unavailable');
    res.destroy();
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.get({ port: server.address().port });
    req.on('response', (res) => res.resume());
    req.on('error', () => {}); // The reset from destroy() is expected
  }));
}
