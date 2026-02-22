'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Integration tests for relaxed header value validation.
// When insecureHTTPParser is enabled, outgoing headers with control characters
// (0x01-0x1f except HTAB, and DEL 0x7f) are allowed per Fetch spec.
// NUL (0x00), CR (0x0d), and LF (0x0a) are always rejected.

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
// Test 2: Client setHeader with control chars in lenient mode - allowed
// ============================================================================
{
  const req = dummyRequest({ insecureHTTPParser: true });
  // Should not throw - control chars allowed in lenient mode
  req.setHeader('X-Test', 'value\x01here');
  req.setHeader('X-Bel', 'ding\x07');
  req.setHeader('X-Esc', 'esc\x1b');
  req.setHeader('X-Del', 'del\x7f');
  req.destroy();
}

// ============================================================================
// Test 3: NUL, CR, LF always rejected even in lenient mode (client)
// ============================================================================
{
  const req = dummyRequest({ insecureHTTPParser: true });
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
// Test 4: Server response setHeader with control chars in lenient mode
// ============================================================================
{
  const server = http.createServer({
    insecureHTTPParser: true,
  }, common.mustCall((req, res) => {
    // Should not throw - control chars allowed in lenient mode
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
// Test 5: Server response NUL/CR/LF always rejected in lenient mode
// ============================================================================
{
  const server = http.createServer({
    insecureHTTPParser: true,
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
// Test 7: appendHeader also respects lenient mode
// ============================================================================
{
  const req = dummyRequest({ insecureHTTPParser: true });
  // Should not throw in lenient mode
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
