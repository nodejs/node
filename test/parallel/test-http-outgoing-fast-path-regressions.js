'use strict';

// Flags: --expose-internals

// Regression coverage for HTTP/1.1 outgoing framing on the JS fast path
// (tryFastEnd / tryFastStoreHeaderObject) and the legacy store/write path.
// The fast path is what ServerResponse uses by default. Re-executing with
// --legacy-path turns it off so the same assertions cover both.

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const { spawnSync } = require('child_process');
const { setOutgoingFastPathForTests } = require('internal/http/outgoing');

const isLegacy = process.argv.includes('--legacy-path');
const pathLabel = isLegacy ? 'legacy' : 'fast';

if (isLegacy) {
  setOutgoingFastPathForTests(false);
}

function rawRequest(handler, callback) {
  const server = http.createServer(common.mustCall(handler));

  server.listen(0, common.mustCall(() => {
    const socket = net.connect(server.address().port, common.mustCall(() => {
      socket.end('GET / HTTP/1.1\r\n' +
                 'Host: localhost\r\n' +
                 'Connection: close\r\n' +
                 '\r\n');
    }));

    socket.setEncoding('latin1');
    let raw = '';
    socket.on('data', (chunk) => { raw += chunk; });
    socket.on('end', common.mustCall(() => {
      server.close(common.mustCall(() => callback(raw)));
    }));
  }));
}

// Empty chunked responses must still terminate the chunked body.
rawRequest((req, res) => {
  res.setHeader('Transfer-Encoding', 'chunked');
  res.end();
}, common.mustCall((raw) => {
  assert.match(raw, /\r\nTransfer-Encoding: chunked\r\n/i, pathLabel);
  assert.match(raw, /\r\n0\r\n\r\n$/, pathLabel);
}));

// Advertising trailers requires chunked framing; do not rewrite to
// Content-Length.
rawRequest((req, res) => {
  res.setHeader('Trailer', 'X-Checksum');
  res.end('ok');
}, common.mustCall((raw) => {
  assert.match(raw, /\r\nTrailer: X-Checksum\r\n/i, pathLabel);
  assert.match(raw, /\r\nTransfer-Encoding: chunked\r\n/i, pathLabel);
  assert.doesNotMatch(raw, /\r\nContent-Length:/i, pathLabel);
  assert.match(raw, /\r\n2\r\nok\r\n0\r\n\r\n$/, pathLabel);
}));

// Transfer-Encoding parameters are valid transfer-coding parameters and
// must still be recognized as chunked framing.
rawRequest((req, res) => {
  res.setHeader('Transfer-Encoding', 'chunked;foo=bar');
  res.end('ok');
}, common.mustCall((raw) => {
  assert.match(raw, /\r\nTransfer-Encoding: chunked;foo=bar\r\n/i, pathLabel);
  assert.doesNotMatch(raw, /\r\nContent-Length:/i, pathLabel);
  assert.match(raw, /\r\n2\r\nok\r\n0\r\n\r\n$/, pathLabel);
}));

// strictContentLength must be enforced for the single res.end(chunk) path
// too. The fast path defers to write_() instead of swallowing the mismatch.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.strictContentLength = true;
    res.setHeader('Content-Length', '4');

    assert.throws(() => {
      res.end('hello');
    }, {
      code: 'ERR_HTTP_CONTENT_LENGTH_MISMATCH',
    });

    res.destroy();
    server.close(common.mustCall());
  }));

  server.listen(0, common.mustCall(() => {
    const socket = net.connect(server.address().port, common.mustCall(() => {
      socket.end('GET / HTTP/1.1\r\n' +
                 'Host: localhost\r\n' +
                 'Connection: close\r\n' +
                 '\r\n');
    }));

    socket.resume();
    socket.on('close', common.mustCall());
  }));
}

// 4294967296 === 2^32. Bookkeeping must keep the full JS number rather than
// wrapping through a 32-bit slot.
{
  const server = http.createServer(common.mustCall((req, res) => {
    res.setHeader('Content-Length', '4294967296');
    res.writeHead(200);

    assert.strictEqual(res._contentLength, 4294967296);

    res.destroy();
    server.close(common.mustCall());
  }));

  server.listen(0, common.mustCall(() => {
    const socket = net.connect(server.address().port, common.mustCall(() => {
      socket.end('GET / HTTP/1.1\r\n' +
                 'Host: localhost\r\n' +
                 'Connection: close\r\n' +
                 '\r\n');
    }));

    socket.resume();
    socket.on('close', common.mustCall());
  }));
}

// Re-run with the fast path disabled so the legacy store/write path is
// checked against the same framing assertions.
if (!isLegacy) {
  const result = spawnSync(
    process.execPath,
    [...process.execArgv, __filename, '--legacy-path'],
    { encoding: 'utf8' },
  );
  if (result.status !== 0) {
    if (result.stdout) process.stdout.write(result.stdout);
    if (result.stderr) process.stderr.write(result.stderr);
    assert.fail(
      `legacy-path re-exec failed with status ${result.status}`,
    );
  }
}
