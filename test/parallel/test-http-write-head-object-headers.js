'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Verify how writeHead() renders a plain headers object, and that the fields
// that drive message framing are recognized in any casing while other names
// are emitted verbatim.

function createResponse({ sendDate = false, keepAliveTimeout = 0 } = {}) {
  const res = new http.ServerResponse({
    method: 'GET',
    httpVersionMajor: 1,
    httpVersionMinor: 1,
  });
  res.sendDate = sendDate;
  res._keepAliveTimeout = keepAliveTimeout;
  return res;
}

function render(headers, options) {
  const res = createResponse(options);
  res.writeHead(200, headers);
  return res;
}

function headerLines(res) {
  const lines = res._header.split('\r\n');
  assert.strictEqual(lines.shift(), 'HTTP/1.1 200 OK');
  // The header block ends with an empty line.
  assert.deepStrictEqual(lines.splice(-2), ['', '']);
  return lines;
}

// Only own enumerable string keys are emitted, in Object.keys() order.
{
  const headers = { __proto__: { inherited: 'no' } };
  headers['x-b'] = '1';
  headers['x-a'] = '2';
  headers[42] = '3';
  Object.defineProperty(headers, 'hidden', { value: 'no', enumerable: false });
  headers[Symbol('symbol')] = 'no';

  assert.deepStrictEqual(headerLines(render(headers)), [
    '42: 3',
    'x-b: 1',
    'x-a: 2',
    'Connection: keep-alive',
    'Transfer-Encoding: chunked',
  ]);
}

// Null-prototype objects work, and accessors are read exactly once.
{
  let reads = 0;
  const headers = { __proto__: null };
  headers['x-a'] = '1';
  Object.defineProperty(headers, 'x-getter', {
    enumerable: true,
    get() {
      reads++;
      return 'v';
    },
  });

  assert.deepStrictEqual(headerLines(render(headers)), [
    'x-a: 1',
    'x-getter: v',
    'Connection: keep-alive',
    'Transfer-Encoding: chunked',
  ]);
  assert.strictEqual(reads, 1);
}

// An object with many headers keeps their order, and a framing header among
// them is still honored.
{
  const headers = {};
  const expected = [];
  for (let i = 0; i < 600; i++) {
    if (i === 300) {
      headers['Content-Length'] = '0';
      expected.push('Content-Length: 0');
    }
    headers[`x-h${i}`] = `v${i}`;
    expected.push(`x-h${i}: v${i}`);
  }

  const res = render(headers);
  assert.deepStrictEqual(headerLines(res),
                         [...expected, 'Connection: keep-alive']);
  assert.strictEqual(res.chunkedEncoding, false);
}

// Framing fields are recognized in any casing.
{
  const res = render({ 'CONTENT-LENGTH': '5' });
  assert.deepStrictEqual(headerLines(res),
                         ['CONTENT-LENGTH: 5', 'Connection: keep-alive']);
  assert.strictEqual(res.chunkedEncoding, false);
}
{
  const res = render({ 'tRaNsFeR-eNcOdInG': 'chunked' });
  assert.deepStrictEqual(headerLines(res),
                         ['tRaNsFeR-eNcOdInG: chunked', 'Connection: keep-alive']);
  assert.strictEqual(res.chunkedEncoding, true);
}
{
  const res = render({ 'cOnNeCtIoN': 'close' });
  assert.deepStrictEqual(headerLines(res),
                         ['cOnNeCtIoN: close', 'Transfer-Encoding: chunked']);
  assert.strictEqual(res._last, true);
}
{
  // A user supplied Date suppresses the automatic one.
  const res = render({ 'DATE': 'x' }, { sendDate: true });
  assert.deepStrictEqual(headerLines(res), [
    'DATE: x',
    'Connection: keep-alive',
    'Transfer-Encoding: chunked',
  ]);
}
{
  // A user supplied Keep-Alive suppresses the automatic one.
  const res = render({ 'kEeP-aLiVe': 'timeout=1' }, { keepAliveTimeout: 5000 });
  assert.deepStrictEqual(headerLines(res), [
    'kEeP-aLiVe: timeout=1',
    'Connection: keep-alive',
    'Transfer-Encoding: chunked',
  ]);
}
{
  // Trailers require chunked encoding.
  assert.throws(() => render({ 'TRAILER': 'x', 'Content-Length': '1' }), {
    code: 'ERR_HTTP_TRAILER_INVALID',
  });
}
{
  // Expect flushes the header right away. There is no socket here, so it is
  // queued on the response.
  assert.strictEqual(render({ 'eXpEcT': '100-continue' }).outputData.length, 1);
}

// Names that share a framing field's length and first letter, or only its
// length, are emitted verbatim and do not affect framing.
{
  const res = render({
    'Content-Lengtx': '5',
    'Xontent-Length': '5',
    'Connectiom': 'close',
    'Transfer-Encodinx': 'identity',
    'Dat3': 'x',
    'Expecx': '100-continue',
    'Trailes': 'x',
    'Keep-Alivx': 'timeout=1',
  }, { sendDate: true, keepAliveTimeout: 5000 });

  const lines = headerLines(res);
  assert.deepStrictEqual(lines.slice(0, 8), [
    'Content-Lengtx: 5',
    'Xontent-Length: 5',
    'Connectiom: close',
    'Transfer-Encodinx: identity',
    'Dat3: x',
    'Expecx: 100-continue',
    'Trailes: x',
    'Keep-Alivx: timeout=1',
  ]);
  assert.match(lines[8], /^Date: .+ GMT$/);
  assert.deepStrictEqual(lines.slice(9), [
    'Connection: keep-alive',
    'Keep-Alive: timeout=5',
    'Transfer-Encoding: chunked',
  ]);
  assert.strictEqual(res.chunkedEncoding, true);
  assert.strictEqual(res._last, false);
  assert.strictEqual(res.outputData.length, 0);
}

// The same rules apply to headers set through setHeader().
{
  const res = createResponse();
  res.setHeader('CONTENT-LENGTH', '0');
  res.setHeader('Content-Lengtx', '1');
  res.writeHead(200);
  assert.deepStrictEqual(headerLines(res), [
    'CONTENT-LENGTH: 0',
    'Content-Lengtx: 1',
    'Connection: keep-alive',
  ]);
  assert.strictEqual(res.chunkedEncoding, false);
}

// End to end: a response with many headers and an upper-case Content-Length.
{
  const headers = {};
  for (let i = 0; i < 600; i++) {
    headers[`x-h${i}`] = `v${i}`;
  }
  headers['CONTENT-LENGTH'] = '5';

  const server = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, headers);
    res.end('hello');
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers['x-h0'], 'v0');
      assert.strictEqual(res.headers['x-h599'], 'v599');
      assert.strictEqual(res.headers['content-length'], '5');
      assert.strictEqual(res.headers['transfer-encoding'], undefined);
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => body += chunk);
      res.on('end', common.mustCall(() => {
        assert.strictEqual(body, 'hello');
        server.close();
      }));
    }));
  }));
}
