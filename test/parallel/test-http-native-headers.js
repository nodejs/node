// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const { IncomingMessage } = http;
const { internalBinding } = require('internal/test/binding');
const {
  HTTPParser,
  nativeHeadersHas,
  nativeHeadersGet,
  nativeHeadersToArray,
  nativeHeadersByteLength,
} = internalBinding('http_parser');

// Headers stay packed until rawHeaders / headers are read.
// Server Host / Expect / body-header checks must not force that.

function packHeaders(pairs, flags = 0) {
  let size = HTTPParser.kNativeHeadersPrefix;
  for (let i = 0; i < pairs.length; i += 2) {
    size += 8 + Buffer.byteLength(pairs[i]) + Buffer.byteLength(pairs[i + 1]);
  }
  const buf = Buffer.alloc(size);
  buf.writeUInt32LE(HTTPParser.kNativeHeadersMagic, 0);
  buf.writeUInt32LE(pairs.length / 2, HTTPParser.kNativeHeadersCountOffset);
  buf.writeUInt32LE(flags, HTTPParser.kNativeHeadersFlagsOffset);
  let p = HTTPParser.kNativeHeadersPrefix;
  for (let i = 0; i < pairs.length; i += 2) {
    const n = Buffer.from(pairs[i], 'latin1');
    const v = Buffer.from(pairs[i + 1], 'latin1');
    buf.writeUInt32LE(n.length, p);
    buf.writeUInt32LE(v.length, p + 4);
    n.copy(buf, p + 8);
    v.copy(buf, p + 8 + n.length);
    p += 8 + n.length + v.length;
  }
  return buf;
}

{
  assert.strictEqual(HTTPParser.kNativeHeadersMagic, 0x5244484E);
  assert.strictEqual(HTTPParser.kNativeHeaderFlagHost, 1 << 0);
  assert.strictEqual(HTTPParser.kNativeHeaderFlagExpect, 1 << 1);
  assert.strictEqual(HTTPParser.kNativeHeaderFlagContentLength, 1 << 2);
  assert.strictEqual(HTTPParser.kNativeHeaderFlagTransferEncoding, 1 << 3);
  assert.strictEqual(HTTPParser.kNativeHeaderFlagTE, 1 << 4);
  assert.strictEqual(HTTPParser.kNativeHeadersCountOffset, 4);
  assert.strictEqual(HTTPParser.kNativeHeadersFlagsOffset, 8);
  assert.strictEqual(HTTPParser.kNativeHeadersPrefix, 12);
}

{
  // Binding helpers: invalid input and crafted little-endian buffers.
  assert.strictEqual(nativeHeadersHas(), false);
  assert.strictEqual(nativeHeadersHas(Buffer.alloc(0), 'host'), false);
  assert.strictEqual(nativeHeadersHas(Buffer.from('xxxx'), 'host'), false);
  assert.strictEqual(nativeHeadersGet(Buffer.alloc(0), 'host'), undefined);
  assert.deepStrictEqual(nativeHeadersToArray(Buffer.alloc(0)), []);
  assert.strictEqual(nativeHeadersByteLength(Buffer.alloc(0)), 0);

  const packed = packHeaders(
    ['Host', 'example.com', 'X-Test', 'one', 'X-Test', 'two', 'TE', 'trailers'],
    HTTPParser.kNativeHeaderFlagHost | HTTPParser.kNativeHeaderFlagTE,
  );
  assert.strictEqual(packed[0], 0x4e);
  assert.strictEqual(packed[1], 0x48);
  assert.strictEqual(packed[2], 0x44);
  assert.strictEqual(packed[3], 0x52);
  assert.strictEqual(nativeHeadersHas(packed, 'host'), true);
  assert.strictEqual(nativeHeadersHas(packed, 'x-test'), true);
  assert.strictEqual(nativeHeadersHas(packed, 'te'), true);
  assert.strictEqual(nativeHeadersHas(packed, 'missing'), false);
  assert.strictEqual(nativeHeadersGet(packed, 'host'), 'example.com');
  assert.strictEqual(nativeHeadersGet(packed, 'x-test'), 'one, two');
  assert.strictEqual(nativeHeadersGet(packed, 'missing'), undefined);
  assert.deepStrictEqual(
    nativeHeadersToArray(packed),
    ['Host', 'example.com', 'X-Test', 'one', 'X-Test', 'two', 'TE', 'trailers'],
  );
  assert.deepStrictEqual(nativeHeadersToArray(packed, 2), ['Host', 'example.com']);
  assert.strictEqual(nativeHeadersByteLength(packed), 8);

  const emptyName = packHeaders(['', 'value', 'X-Empty', '']);
  assert.deepStrictEqual(nativeHeadersToArray(emptyName), ['', 'value', 'X-Empty', '']);
  assert.strictEqual(nativeHeadersGet(emptyName, 'x-empty'), '');
  assert.strictEqual(nativeHeadersHas(emptyName, ''), true);

  const truncated = packed.subarray(0, 14);
  assert.strictEqual(nativeHeadersHas(truncated, 'host'), false);
  assert.deepStrictEqual(nativeHeadersToArray(truncated), []);
}

{
  // Fresh IncomingMessage keeps rawHeaders as an own data property.
  const fresh = new IncomingMessage();
  assert.strictEqual(Object.hasOwn(fresh, 'rawHeaders'), true);
  assert.deepStrictEqual(fresh.rawHeaders, []);
  const desc = Object.getOwnPropertyDescriptor(fresh, 'rawHeaders');
  assert.strictEqual(desc.writable, true);
  assert.strictEqual(desc.enumerable, true);
}

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  switch (req.url) {
    case '/lazy': {
      assert.strictEqual(Object.hasOwn(req, 'rawHeaders'), true);
      assert.strictEqual(req._hasHeader('host'), true);
      assert.strictEqual(req._hasHeader('x-test'), true);
      assert.strictEqual(req._hasHeader('x-missing'), false);
      assert.strictEqual(req._getHeader('x-test'), 'one, two');
      assert.strictEqual(req._hasBodyHeaders(), false);
      assert.strictEqual(req._hasHeader('te'), false);

      // First public access materializes JS strings and becomes a data property.
      assert.strictEqual(req.headers.host, `localhost:${server.address().port}`);
      assert.strictEqual(req.headers['x-test'], 'one, two');
      assert.ok(Array.isArray(req.rawHeaders));
      assert.ok(req.rawHeaders.includes('X-Test'));
      const after = Object.getOwnPropertyDescriptor(req, 'rawHeaders');
      assert.strictEqual(after.writable, true);
      assert.strictEqual(typeof after.get, 'undefined');
      // Fallback scans after the packed buffer is gone.
      assert.strictEqual(req._hasHeader('host'), true);
      assert.strictEqual(req._hasHeader('x-test'), true);
      assert.strictEqual(req._hasHeader('x-missing'), false);
      assert.strictEqual(req._getHeader('x-test'), 'one, two');
      assert.strictEqual(req._getHeader('x-missing'), undefined);
      assert.strictEqual(req._hasBodyHeaders(), false);
      res.end('ok');
      break;
    }
    case '/length': {
      assert.strictEqual(req._hasBodyHeaders(), true);
      assert.strictEqual(req._hasHeader('content-length'), true);
      assert.strictEqual(req._getHeader('content-length'), '0');
      // Materialize, then hit the rawHeaders fallback path.
      assert.ok(req.rawHeaders.includes('Content-Length') ||
                req.rawHeaders.some((h) => h.toLowerCase() === 'content-length'));
      assert.strictEqual(req._hasBodyHeaders(), true);
      res.end();
      break;
    }
    case '/chunked': {
      assert.strictEqual(req._hasBodyHeaders(), true);
      assert.strictEqual(req._hasHeader('transfer-encoding'), true);
      req.resume();
      req.on('end', common.mustCall(() => {
        assert.ok(req.rawHeaders.some((h) => h.toLowerCase() === 'transfer-encoding'));
        assert.strictEqual(req._hasBodyHeaders(), true);
        assert.strictEqual(req._hasHeader('transfer-encoding'), true);
        res.end('ok');
      }));
      break;
    }
    case '/te': {
      assert.strictEqual(req._hasHeader('te'), true);
      assert.strictEqual(req._getHeader('te'), 'trailers');
      assert.ok(req.rawHeaders.some((h) => h.toLowerCase() === 'te'));
      assert.strictEqual(req._hasHeader('te'), true);
      assert.strictEqual(req._getHeader('te'), 'trailers');
      res.end('ok');
      break;
    }
    case '/expect': {
      assert.strictEqual(req._hasHeader('expect'), true);
      assert.strictEqual(req._getHeader('expect'), '100-continue');
      assert.ok(req.rawHeaders.some((h) => h.toLowerCase() === 'expect'));
      assert.strictEqual(req._hasHeader('expect'), true);
      assert.strictEqual(req._getHeader('expect'), '100-continue');
      res.end('ok');
      break;
    }
    case '/mixed-length': {
      assert.strictEqual(req._hasBodyHeaders(), true);
      const names = req.rawHeaders.filter((_, i) => i % 2 === 0);
      assert.ok(names.some((n) => n.toLowerCase() === 'content-length'));
      assert.strictEqual(req._hasBodyHeaders(), true);
      res.end();
      break;
    }
    case '/assign-raw': {
      req.rawHeaders = ['X-Assigned', 'yes'];
      assert.deepStrictEqual(req.rawHeaders, ['X-Assigned', 'yes']);
      res.end('ok');
      break;
    }
    default:
      res.writeHead(404);
      res.end();
  }
}, 7));

server.listen(0, common.mustSucceed(async () => {
  const port = server.address().port;

  function get(path, headers = {}) {
    return new Promise((resolve, reject) => {
      http.get({ port, path, headers }, (res) => {
        res.resume();
        res.on('end', resolve);
        res.on('error', reject);
      }).on('error', reject);
    });
  }

  try {
    await get('/lazy', { 'X-Test': ['one', 'two'] });
    await get('/length', { 'Content-Length': '0' });

    await new Promise((resolve, reject) => {
      const req = http.request({
        port,
        path: '/chunked',
        method: 'POST',
        headers: { 'Transfer-Encoding': 'chunked' },
      }, (res) => {
        res.resume();
        res.on('end', resolve);
        res.on('error', reject);
      });
      req.on('error', reject);
      req.write('hi');
      req.end();
    });

    await get('/te', { TE: 'trailers' });
    await get('/expect', { Expect: '100-continue' });
    await get('/mixed-length', { 'CONTENT-LENGTH': '0' });
    await get('/assign-raw');

    await new Promise((resolve, reject) => {
      const client = net.connect(port, () => {
        client.write('GET / HTTP/1.1\r\n\r\n');
      });
      const chunks = [];
      client.on('data', (c) => chunks.push(c));
      client.on('end', common.mustCall(() => {
        const raw = Buffer.concat(chunks).toString('latin1');
        assert.match(raw, /^HTTP\/1\.1 400 /);
        resolve();
      }));
      client.on('error', reject);
    });
  } finally {
    server.close();
  }
}));
