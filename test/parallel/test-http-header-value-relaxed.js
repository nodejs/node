// Flags: --expose-internals --no-warnings

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const { duplexPair } = require('stream');
const { HTTPParser } = require('internal/http/common');
// llhttp_set_lenient_header_value_relaxed() was added in llhttp 9.4.0.
// On shared-library builds using an older system llhttp the constant is
// exported as 0, so inbound-parsing tests must be skipped there.
const kRelaxedInboundSupported = HTTPParser.kLenientHeaderValueRelaxed > 0;

// Integration tests for relaxed header value validation.
// When httpValidation is 'relaxed' or 'insecure', outgoing headers with control
// characters (0x01-0x1f except HTAB, and DEL 0x7f) are allowed per Fetch spec.
// NUL (0x00), CR (0x0d), and LF (0x0a) are always rejected.
// httpValidation: 'relaxed' - only enables relaxed header value parsing, not
//   other insecure lenient behaviours (e.g. duplicate Transfer-Encoding).
// httpValidation: 'insecure' - enables all lenient parsing (same as insecureHTTPParser).
// httpValidation and insecureHTTPParser are mutually exclusive.

// Helper: create a request that won't actually connect (for setHeader tests)
function dummyRequest(opts) {
  const req = http.request({ host: '127.0.0.1', port: 1, ...opts });
  req.on('error', () => {});  // Suppress connection errors
  return req;
}

// ============================================================================
// Test 1: Client setHeader with control chars in strict mode (default) - throws
// ============================================================================
{
  const req = dummyRequest();
  assert.throws(() => {
    req.setHeader('X-Test', 'value\x01here');
  }, { code: 'ERR_INVALID_CHAR' });
  req.destroy();
}

// ============================================================================
// Test 2: Client setHeader with control chars in relaxed mode - allowed
// ============================================================================
{
  const req = dummyRequest({ httpValidation: 'relaxed' });
  // Should not throw - control chars allowed in relaxed mode
  req.setHeader('X-Test', 'value\x01here');
  req.setHeader('X-Bel', 'ding\x07');
  req.setHeader('X-Esc', 'esc\x1b');
  req.setHeader('X-Del', 'del\x7f');
  req.destroy();
}

// ============================================================================
// Test 3: NUL, CR, LF always rejected even in relaxed mode (client)
// ============================================================================
{
  const req = dummyRequest({ httpValidation: 'relaxed' });
  assert.throws(() => {
    req.setHeader('X-Test', 'value\x00here');
  }, { code: 'ERR_INVALID_CHAR' });
  assert.throws(() => {
    req.setHeader('X-Test', 'value\rhere');
  }, { code: 'ERR_INVALID_CHAR' });
  assert.throws(() => {
    req.setHeader('X-Test', 'value\nhere');
  }, { code: 'ERR_INVALID_CHAR' });
  req.destroy();
}

// ============================================================================
// Test 4: Server response setHeader with control chars in relaxed mode
// ============================================================================
{
  const server = http.createServer({
    httpValidation: 'relaxed',
  }, common.mustCall((req, res) => {
    // Should not throw - control chars allowed in relaxed mode
    res.setHeader('X-Custom', 'value\x01here');
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    // Use a raw TCP connection to read the response headers directly,
    // since http.get would fail to parse the control char in the header.
    const client = net.connect(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));
    let data = '';
    client.on('data', (chunk) => { data += chunk; });
    client.on('end', common.mustCall(() => {
      // eslint-disable-next-line no-control-regex
      assert.match(data, /X-Custom: value\x01here/);
      server.close();
    }));
  }));
}

// ============================================================================
// Test 5: Server response NUL/CR/LF always rejected in relaxed mode
// ============================================================================
{
  const server = http.createServer({
    httpValidation: 'relaxed',
  }, common.mustCall((req, res) => {
    assert.throws(() => {
      res.setHeader('X-Test', 'value\x00here');
    }, { code: 'ERR_INVALID_CHAR' });
    assert.throws(() => {
      res.setHeader('X-Test', 'value\rhere');
    }, { code: 'ERR_INVALID_CHAR' });
    assert.throws(() => {
      res.setHeader('X-Test', 'value\nhere');
    }, { code: 'ERR_INVALID_CHAR' });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}

// ============================================================================
// Test 6: Server response strict mode (default) rejects control chars
// ============================================================================
{
  const server = http.createServer(common.mustCall((req, res) => {
    assert.throws(() => {
      res.setHeader('X-Test', 'value\x01here');
    }, { code: 'ERR_INVALID_CHAR' });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}

// ============================================================================
// Test 7: appendHeader also respects relaxed mode
// ============================================================================
{
  const req = dummyRequest({ httpValidation: 'relaxed' });
  // Should not throw in relaxed mode
  req.appendHeader('X-Test', 'value\x01here');
  req.destroy();
}

// ============================================================================
// Test 8: appendHeader strict mode rejects control chars
// ============================================================================
{
  const req = dummyRequest();
  assert.throws(() => {
    req.appendHeader('X-Test', 'value\x01here');
  }, { code: 'ERR_INVALID_CHAR' });
  req.destroy();
}

// ============================================================================
// Test 9: Explicit insecureHTTPParser: false overrides global flag
// ============================================================================
{
  const req = dummyRequest({ insecureHTTPParser: false });
  assert.throws(() => {
    req.setHeader('X-Test', 'value\x01here');
  }, { code: 'ERR_INVALID_CHAR' });
  req.destroy();
}

// ============================================================================
// Test 10: Inbound response header with control char accepted in lenient mode
//          (exercises the new llhttp_set_lenient_header_value_relaxed path)
// Only runs on builds with llhttp >= 9.4 (kLenientHeaderValueRelaxed > 0).
// ============================================================================
if (kRelaxedInboundSupported) {
  const [clientSide, serverSide] = duplexPair();

  const req = http.request({
    createConnection: common.mustCall(() => clientSide),
    httpValidation: 'relaxed',
  }, common.mustCall((res) => {
    assert.strictEqual(res.headers['x-ctrl'], 'value\x01here');
    res.resume();
    res.on('end', common.mustCall());
  }));
  req.end();

  serverSide.resume();
  serverSide.end(
    'HTTP/1.1 200 OK\r\n' +
    'X-Ctrl: value\x01here\r\n' +
    'Content-Length: 0\r\n' +
    '\r\n',
  );
}

// Test 10b: Same inbound header without insecureHTTPParser — parser must error
{
  const [clientSide, serverSide] = duplexPair();

  const req = http.request({
    createConnection: common.mustCall(() => clientSide),
  }, common.mustNotCall());
  req.end();
  req.on('error', common.mustCall());

  serverSide.resume();
  serverSide.end(
    'HTTP/1.1 200 OK\r\n' +
    'X-Ctrl: value\x01here\r\n' +
    'Content-Length: 0\r\n' +
    '\r\n',
  );
}

// ============================================================================
// Test 11: httpValidation: 'insecure' outbound - same as insecureHTTPParser
// ============================================================================
{
  const req = dummyRequest({ httpValidation: 'insecure' });
  // Should not throw - control chars allowed in insecure mode
  req.setHeader('X-Test', 'value\x01here');
  req.setHeader('X-Bel', 'ding\x07');
  req.destroy();
}

// ============================================================================
// Test 12: Mutual exclusion - client throws when both options are set
// ============================================================================
{
  assert.throws(() => {
    dummyRequest({ httpValidation: 'relaxed', insecureHTTPParser: true });
  }, { code: 'ERR_INVALID_ARG_VALUE' });
}

// ============================================================================
// Test 13: Mutual exclusion - server throws when both options are set
// ============================================================================
{
  assert.throws(() => {
    http.createServer({ httpValidation: 'relaxed', insecureHTTPParser: true });
  }, { code: 'ERR_INVALID_ARG_VALUE' });
}

// ============================================================================
// Test 14: httpValidation: 'relaxed' accepts inbound REQUEST headers with
//          control chars (server side - exercises kLenientHeaderValueRelaxed)
// Only runs on builds with llhttp >= 9.4 (kLenientHeaderValueRelaxed > 0).
// ============================================================================
if (kRelaxedInboundSupported) {
  const server = http.createServer({
    httpValidation: 'relaxed',
  }, common.mustCall((req, res) => {
    assert.strictEqual(req.headers['x-ctrl'], 'value\x01here');
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    // Use a raw TCP connection to send a request with a control char header.
    const client = net.connect(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nX-Ctrl: value\x01here\r\nConnection: close\r\n\r\n');
    }));
    let data = '';
    client.on('data', (chunk) => { data += chunk; });
    client.on('end', common.mustCall(() => {
      assert.match(data, /^HTTP\/1\.1 200/);
      server.close();
    }));
  }));
}

// ============================================================================
// Test 15: httpValidation: 'relaxed' inbound REQUEST - strict mode (default)
//          rejects request with control char in header value
// ============================================================================
{
  const server = http.createServer(
    common.mustNotCall(),
  );

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.connect(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nX-Ctrl: value\x01here\r\nConnection: close\r\n\r\n');
    }));
    let data = '';
    client.on('data', (chunk) => { data += chunk; });
    client.on('end', common.mustCall(() => {
      // Server should respond with 400 Bad Request or close the connection
      assert.match(data, /^HTTP\/1\.1 400|^$/);
      server.close();
    }));
  }));
}

// ============================================================================
// Test 16: httpValidation: 'relaxed' does NOT enable all insecure lenient
//          flags - duplicate Transfer-Encoding is still rejected in relaxed
//          mode but accepted in insecure mode.
// (kLenientTransferEncoding is only in kLenientAll, not kLenientHeaderValueRelaxed)
// ============================================================================
{
  // A request where Transfer-Encoding: chunked appears twice (joined internally
  // as "chunked, chunked"), which llhttp rejects without kLenientTransferEncoding.
  const doubleTE =
    'GET / HTTP/1.1\r\n' +
    'Host: localhost\r\n' +
    'Connection: close\r\n' +
    'Transfer-Encoding: chunked\r\n' +
    'Transfer-Encoding: chunked\r\n' +
    '\r\n' +
    '0\r\n\r\n';

  // With httpValidation: 'relaxed', duplicate T-E should be rejected (400).
  {
    const server = http.createServer({
      httpValidation: 'relaxed',
    }, common.mustNotCall());

    server.listen(0, common.mustCall(() => {
      const port = server.address().port;
      const client = net.connect(port, common.mustCall(() => {
        client.write(doubleTE);
      }));
      client.resume();
      client.on('close', common.mustCall(() => {
        server.close();
      }));
    }));
  }

  // With httpValidation: 'insecure', duplicate T-E is accepted (kLenientAll
  // includes kLenientTransferEncoding).
  {
    const server = http.createServer({
      httpValidation: 'insecure',
    }, common.mustCall((req, res) => {
      res.end('ok');
    }));

    server.listen(0, common.mustCall(() => {
      const port = server.address().port;
      const client = net.connect(port, common.mustCall(() => {
        client.write(doubleTE);
      }));
      let data = '';
      client.on('data', (chunk) => { data += chunk; });
      client.on('end', common.mustCall(() => {
        assert.match(data, /^HTTP\/1\.1 200/);
        server.close();
      }));
    }));
  }
}

// ============================================================================
// Test 17: writeHead respects httpValidation: 'relaxed'
//          (exercises the storeHeader/validateHeaderValue path, not setHeader)
// ============================================================================
{
  const server = http.createServer({
    httpValidation: 'relaxed',
  }, common.mustCall((req, res) => {
    // writeHead calls _storeHeader which calls storeHeader/validateHeaderValue.
    // With httpValidation: 'relaxed', control chars should be allowed.
    res.writeHead(200, { 'X-Custom': 'value\x01here' });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = net.connect(port, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n');
    }));
    let data = '';
    client.on('data', (chunk) => { data += chunk; });
    client.on('end', common.mustCall(() => {
      // eslint-disable-next-line no-control-regex
      assert.match(data, /X-Custom: value\x01here/);
      server.close();
    }));
  }));
}

// ============================================================================
// Test 18: writeHead strict mode (default) rejects control chars
// ============================================================================
{
  const server = http.createServer(common.mustCall((req, res) => {
    assert.throws(() => {
      res.writeHead(200, { 'X-Custom': 'value\x01here' });
    }, { code: 'ERR_INVALID_CHAR' });
    res.end('ok');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}

// ============================================================================
// Test 19: httpValidation: 'strict' (explicit) rejects control chars even
//          when insecureHTTPParser would otherwise be lenient
// ============================================================================
{
  const req = dummyRequest({ httpValidation: 'strict' });
  // 'strict' must always reject control chars, regardless of any global setting
  assert.throws(() => {
    req.setHeader('X-Test', 'value\x01here');
  }, { code: 'ERR_INVALID_CHAR' });
  req.destroy();
}
