'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('node:assert/strict');
const http2 = require('node:http2');
const { once } = require('node:events');

// Content-Encoding is a list (RFC 9110 section 8.4). Preserve all codings
// in their original order, regardless of single-value field validation.
async function test(options, value) {
  const values = Array.isArray(value) ? value : [value];
  const expectedRaw = values.flatMap((v) => ['content-encoding', v]);
  const expected = values.join(', ');

  function check(headers, rawHeaders) {
    assert.strictEqual(headers['content-encoding'], expected);
    const raw = [];
    for (let i = 0; i < rawHeaders.length; i += 2) {
      if (rawHeaders[i] === 'content-encoding')
        raw.push(rawHeaders[i], rawHeaders[i + 1]);
    }
    assert.deepStrictEqual(raw, expectedRaw);
  }

  const server = http2.createServer(options);
  server.on('stream', common.mustCall((stream, headers, flags, rawHeaders) => {
    check(headers, rawHeaders);
    stream.respond({ 'content-encoding': value });
    stream.end();
  }));
  server.listen(0);
  await once(server, 'listening');

  const client = http2.connect(`http://localhost:${server.address().port}`, options);
  const req = client.request({ 'content-encoding': value });
  req.on('response', common.mustCall((headers, flags, rawHeaders) => {
    check(headers, rawHeaders);
  }));
  req.resume();
  req.end();
  await once(req, 'end');
  client.close();
  await once(client, 'close');
  await new Promise((resolve) => server.close(resolve));
}

(async () => {
  for (const options of [{}, { strictSingleValueFields: true },
                         { strictSingleValueFields: false }]) {
    for (const value of ['gzip', 'gzip, br', ['gzip', 'br'],
                         ['br', 'gzip'], ['gzip', 'gzip']]) {
      await test(options, value);
    }
  }
})().then(common.mustCall());
