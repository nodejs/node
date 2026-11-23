'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// The length announced for a chunk (or in Content-Length) must match the
// number of bytes actually written, also for encodings where the decoded size
// is smaller than the one estimated by `Buffer.byteLength()`.

const cases = [
  // [string, encoding, expected decoded bytes]
  ['QUJD    REVG', 'base64', 'ABCDEF'],
  ['QUJD\r\n\r\nREVG', 'BASE64', 'ABCDEF'],
  ['QUJD    REVG', 'base64url', 'ABCDEF'],
  ['41424  3', 'hex', 'AB'],
  ['4142zz4344', 'hex', 'AB'],
  ['414', 'hex', 'A'],
];

// Reads the raw request, returning the body bytes de-framed according to
// the framing used. Fails if the framing is not exactly consumed.
function parseChunked(raw) {
  let rest = raw;
  let body = '';
  for (;;) {
    const eol = rest.indexOf('\r\n');
    assert.notStrictEqual(eol, -1);
    const size = parseInt(rest.slice(0, eol), 16);
    rest = rest.slice(eol + 2);
    if (size === 0) {
      assert.strictEqual(rest, '\r\n');
      return body;
    }
    body += rest.slice(0, size);
    assert.strictEqual(rest.slice(size, size + 2), '\r\n');
    rest = rest.slice(size + 2);
  }
}

function run(send, isComplete, verify) {
  const server = net.createServer(common.mustCall((socket) => {
    let data = '';
    socket.setEncoding('latin1');
    socket.on('data', (chunk) => {
      data += chunk;
      const idx = data.indexOf('\r\n\r\n');
      if (idx === -1) return;
      const head = data.slice(0, idx);
      const raw = data.slice(idx + 4);
      if (isComplete(head, raw)) {
        verify(head, raw);
        socket.end('HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n');
      }
    });
  }));

  server.listen(0, common.mustCall(() => {
    const req = http.request({ port: server.address().port, method: 'POST', agent: false },
                             common.mustCall((res) => {
                               res.resume();
                               res.on('end', common.mustCall(() => server.close()));
                             }));
    send(req);
  }));
}

for (const [str, encoding, expected] of cases) {
  // Chunked, write() then end()
  run((req) => {
    req.write(str, encoding);
    req.end();
  }, (head, raw) => raw.endsWith('0\r\n\r\n'), common.mustCall((head, raw) => {
    assert.match(head, /Transfer-Encoding: chunked/i);
    assert.strictEqual(parseChunked(raw), expected);
  }));

  // Chunked, write() twice so a following chunk would be mis-framed
  run((req) => {
    req.write(str, encoding);
    req.write('tail');
    req.end();
  }, (head, raw) => raw.endsWith('0\r\n\r\n'), common.mustCall((head, raw) => {
    assert.strictEqual(parseChunked(raw), `${expected}tail`);
  }));

  // Single final chunk: Content-Length is computed
  run((req) => {
    req.end(str, encoding);
  }, (head, raw) => raw.length >= expected.length, common.mustCall((head, raw) => {
    const match = /Content-Length: (\d+)/i.exec(head);
    assert.ok(match);
    assert.strictEqual(Number(match[1]), expected.length);
    assert.strictEqual(raw, expected);
  }));
}
