'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const { OutgoingMessage } = http;

// `outputSize`, and the per-connection counter fed by `_onPendingData()`,
// track how many *bytes* are buffered on an outgoing message. They are used
// to decide when to apply backpressure, so measuring a string by its number
// of UTF-16 code units instead of its byte length made Node under-account any
// multi-byte body and apply backpressure too late.
// Refs: https://github.com/nodejs/node/issues/57985

// An OutgoingMessage without a socket buffers everything it is handed, which
// is the path that does the accounting.
function createMessage(options) {
  const msg = new OutgoingMessage(options);
  msg._implicitHeader = function() {};
  return msg;
}

// A two byte character is counted as two bytes, not as one character.
{
  const msg = createMessage();
  assert.strictEqual(msg.write('é'.repeat(100)), true);
  assert.strictEqual(msg.outputSize, 200);
}

// Characters outside the BMP are a surrogate pair in UTF-16 (two code units)
// and four bytes in UTF-8.
{
  const msg = createMessage();
  msg.write('😀'.repeat(10));
  assert.strictEqual(msg.outputSize, 40);
}

// Plain ASCII is unaffected: one code unit is one byte.
{
  const msg = createMessage();
  msg.write('a'.repeat(100));
  assert.strictEqual(msg.outputSize, 100);
}

// Buffers and other views already report a byte length.
{
  const msg = createMessage();
  const chunk = Buffer.from('é'.repeat(100), 'utf8');
  assert.strictEqual(chunk.length, 200);
  msg.write(chunk);
  assert.strictEqual(msg.outputSize, 200);
}

// The declared encoding decides the byte length: the same string is 100 bytes
// as latin1 and 200 bytes as utf8.
{
  const msg = createMessage();
  msg.write('é'.repeat(100), 'latin1');
  assert.strictEqual(msg.outputSize, 100);
}

// Encodings that decode to fewer bytes than the string has characters are
// counted by what they decode to, not by the length of the source string.
{
  const msg = createMessage();
  msg.write('deadbeef', 'hex');
  assert.strictEqual(msg.outputSize, 4);
}

{
  const msg = createMessage();
  msg.write('AAAAAA==', 'base64');
  assert.strictEqual(msg.outputSize, Buffer.byteLength('AAAAAA==', 'base64'));
}

// The per-connection counter is fed the same byte length.
{
  const msg = createMessage();
  const deltas = [];
  msg._onPendingData = (delta) => deltas.push(delta);
  msg.write('é'.repeat(100));
  assert.deepStrictEqual(deltas, [200]);
}

// The header is prepended to the first string chunk, and has to be accounted
// for along with it.
{
  const header = 'HTTP/1.1 200 OK\r\n\r\n';
  const msg = createMessage();
  msg._implicitHeader = function() { this._header = header; };
  msg.write('é'.repeat(100));
  assert.strictEqual(msg.outputSize, Buffer.byteLength(header) + 200);
}

// The header is also accounted for when the caller already knows the byte
// length of the body, as is the case for chunked encoding. The bytes it
// contributes must not be dropped in favour of the body length alone.
{
  const header = 'HTTP/1.1 200 OK\r\n\r\n';
  const msg = createMessage();
  msg._implicitHeader = function() { this._header = header; };
  msg.chunkedEncoding = true;
  msg.write('é'.repeat(100));
  // Header + "c8" + CRLF + 200 bytes of body + CRLF.
  assert.strictEqual(msg.outputSize,
                     Buffer.byteLength(header) + 2 + 2 + 200 + 2);
}

// Chunked encoding without a header: the byte length handed to `_send()` is
// the one that gets used, rather than the body being measured again.
{
  const msg = createMessage();
  msg.chunkedEncoding = true;
  msg.write('é'.repeat(100));
  assert.strictEqual(msg.outputSize, 2 + 2 + 200 + 2);
}

// Backpressure kicks in once the buffered *bytes* reach the high water mark.
// 50 two-byte characters are exactly 100 bytes, so the message is full.
{
  const msg = createMessage({ highWaterMark: 100 });
  assert.strictEqual(msg.writableHighWaterMark, 100);
  const ret = msg.write('é'.repeat(50));
  assert.strictEqual(msg.outputSize, 100);
  assert.strictEqual(ret, false);
  assert.strictEqual(msg.writableNeedDrain, true);
}

// The same number of single-byte characters is only half as much data, so it
// still fits.
{
  const msg = createMessage({ highWaterMark: 100 });
  const ret = msg.write('a'.repeat(50));
  assert.strictEqual(msg.outputSize, 50);
  assert.strictEqual(ret, true);
  assert.strictEqual(msg.writableNeedDrain, false);
}

// `writableLength` reports the buffered byte count.
{
  const msg = createMessage();
  msg.write('é'.repeat(100));
  assert.strictEqual(msg.writableLength, 200);
}

// Flushing hands back exactly what was accounted for, leaving the counters at
// zero rather than drifting.
{
  const msg = createMessage();
  let pending = 0;
  msg._onPendingData = (delta) => { pending += delta; };
  msg.write('é'.repeat(100));
  msg.write('😀'.repeat(10));
  assert.strictEqual(pending, 240);
  assert.strictEqual(msg.outputSize, 240);

  const written = [];
  msg._flushOutput({
    cork() {},
    uncork() {},
    write(data, encoding) { written.push([data, encoding]); },
  });
  assert.strictEqual(msg.outputSize, 0);
  assert.strictEqual(pending, 0);
  assert.strictEqual(written.length, 2);
}

// End to end: correcting the accounting must not change what is put on the
// wire. A chunked multi-byte body goes through the corked path, where the
// chunk is handed to `_send()` without a precomputed length, so this also
// exercises measuring the string inside `_writeRaw()`.
{
  const body = '😀é漢字'.repeat(2000);
  const server = http.createServer(common.mustCall((req, res) => {
    res.setHeader('Content-Type', 'text/plain; charset=utf-8');
    // Without a content-length the response is chunked.
    res.write(body);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    http.get({ port: server.address().port }, common.mustCall((res) => {
      assert.strictEqual(res.headers['transfer-encoding'], 'chunked');
      const chunks = [];
      res.on('data', (chunk) => chunks.push(chunk));
      res.on('end', common.mustCall(() => {
        const received = Buffer.concat(chunks);
        assert.strictEqual(received.length, Buffer.byteLength(body));
        assert.strictEqual(received.toString('utf8'), body);
        server.close();
      }));
    }));
  }));
}
