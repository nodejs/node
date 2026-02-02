'use strict';

// Tests the ability to minimally request a byte range with respondWithFD

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const http2 = require('http2');
const assert = require('assert');
const fs = require('fs');
const Countdown = require('../common/countdown');

const {
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_CONTENT_LENGTH
} = http2.constants;

const fname = fixtures.path('printA.js');
const data = fs.readFileSync(fname);
const fd = fs.openSync(fname, 'r');

// Note: this is not anywhere close to a proper implementation of the range
// header.
function getOffsetLength(range) {
  if (range === undefined)
    return [0, -1];
  const r = /bytes=(\d+)-(\d+)/.exec(range);
  return [+r[1], +r[2] - +r[1]];
}

const server = http2.createServer();
server.on('stream', (stream, headers) => {

  const [ offset, length ] = getOffsetLength(headers.range);

  stream.respondWithFD(fd, {
    [HTTP2_HEADER_CONTENT_TYPE]: 'text/plain'
  }, {
    statCheck: common.mustCall((stat, headers, options) => {
      assert.strictEqual(options.length, length);
      assert.strictEqual(options.offset, offset);
      headers['content-length'] =
        Math.min(options.length, stat.size - offset);
    }),
    offset: offset,
    length: length
  });
});
server.on('close', common.mustCall(() => fs.closeSync(fd)));

server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(2, () => {
    client.close();
    server.close();
  });

  {
    const req = client.request({ range: 'bytes=8-11' });

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers['content-type'], 'text/plain');
      assert.strictEqual(+headers['content-length'], 3);
    }));
    req.setEncoding('utf8');
    let check = '';
    req.on('data', (chunk) => check += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(check, data.toString('utf8', 8, 11));
    }));
    req.on('close', common.mustCall(() => countdown.dec()));
    req.end();
  }

  {
    const req = client.request({ range: 'bytes=8-28' });

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[HTTP2_HEADER_CONTENT_TYPE], 'text/plain');
      assert.strictEqual(+headers[HTTP2_HEADER_CONTENT_LENGTH], 9);
    }));
    req.setEncoding('utf8');
    let check = '';
    req.on('data', (chunk) => check += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(check, data.toString('utf8', 8, 28));
    }));
    req.on('close', common.mustCall(() => countdown.dec()));
    req.end();
  }

});
