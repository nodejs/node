'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Verify that res.end() with chunked encoding still produces a valid
// HTTP/1.1 message when headers + last chunk + terminator are combined
// into a single write.

function rawRequest(port, path) {
  return new Promise((resolve, reject) => {
    const client = net.connect(port, () => {
      client.write(
        `GET ${path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n`);
    });
    const chunks = [];
    client.on('data', (c) => chunks.push(c));
    client.on('end', () => resolve(Buffer.concat(chunks)));
    client.on('error', reject);
  });
}

function assertChunkedBody(raw, expected) {
  const expectedBytes = Buffer.from(expected, 'utf8');
  const rawStr = raw.toString('latin1');
  const sep = rawStr.indexOf('\r\n\r\n');
  assert.notStrictEqual(sep, -1, `missing header separator: ${rawStr}`);
  const body = raw.subarray(sep + 4);
  const hex = expectedBytes.byteLength.toString(16);
  const expectedBody = Buffer.concat([
    Buffer.from(`${hex}\r\n`),
    expectedBytes,
    Buffer.from('\r\n0\r\n\r\n'),
  ]);
  assert.deepStrictEqual(body, expectedBody);
}

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  switch (req.url) {
    case '/string':
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('hello');
      break;
    case '/buffer':
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end(Buffer.from('world'));
      break;
    case '/utf8':
      res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('héllo');
      break;
    case '/trailer':
      res.writeHead(200, {
        'Content-Type': 'text/plain',
        'Trailer': 'X-Test',
      });
      res.addTrailers({ 'X-Test': 'ok' });
      res.end('bye');
      break;
    case '/unusual-length':
      res.writeHead(200, { 'CONTENT-LENGTH': '2' });
      res.end('hi');
      break;
    case '/latin1-header':
      res.writeHead(200, [
        'content-disposition',
        Buffer.from('bår').toString('binary'),
      ]);
      res.end('ok');
      break;
    default:
      res.writeHead(404);
      res.end();
  }
}, 6));

server.listen(0, common.mustCall(async () => {
  const port = server.address().port;

  const stringRaw = await rawRequest(port, '/string');
  const stringText = stringRaw.toString('latin1');
  assert.match(stringText, /^HTTP\/1\.1 200 OK\r\n/);
  assert.match(stringText, /Transfer-Encoding: chunked\r\n/i);
  assertChunkedBody(stringRaw, 'hello');

  const bufferRaw = await rawRequest(port, '/buffer');
  assertChunkedBody(bufferRaw, 'world');

  const utf8Raw = await rawRequest(port, '/utf8');
  assertChunkedBody(utf8Raw, 'héllo');

  const trailerRaw = await rawRequest(port, '/trailer');
  const trailerText = trailerRaw.toString('latin1');
  const trailerSep = trailerText.indexOf('\r\n\r\n');
  const trailerBody = trailerText.slice(trailerSep + 4);
  assert.strictEqual(trailerBody, '3\r\nbye\r\n0\r\nX-Test: ok\r\n\r\n');

  const lengthRaw = await rawRequest(port, '/unusual-length');
  const lengthText = lengthRaw.toString('latin1');
  assert.match(lengthText, /CONTENT-LENGTH: 2\r\n/);
  assert.ok(lengthText.endsWith('\r\n\r\nhi'));

  const latin1Raw = await rawRequest(port, '/latin1-header');
  const latin1Text = latin1Raw.toString('latin1');
  const expectedLatin1 = Buffer.from('bår').toString('latin1');
  assert.ok(
    latin1Text.includes(`content-disposition: ${expectedLatin1}\r\n`),
    latin1Text,
  );
  assertChunkedBody(latin1Raw, 'ok');

  server.close();
}));
