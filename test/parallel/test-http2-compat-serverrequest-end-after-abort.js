'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A request whose stream is reset by the peer before END_STREAM has an
// incomplete body. Like http1, the compat request must not emit 'end' for it.

const assert = require('assert');
const http2 = require('http2');
const { finished } = require('stream');

const codes = [
  http2.constants.NGHTTP2_NO_ERROR,
  http2.constants.NGHTTP2_CANCEL,
];

const server = http2.createServer(common.mustCall((req, res) => {
  // Tell the client the partial body arrived, so it can reset the stream.
  req.once('data', common.mustCall(() => res.writeHead(200).flushHeaders()));
  req.on('end', common.mustNotCall());
  req.on('aborted', common.mustCall());
  req.on('error', common.expectsError({ code: 'ERR_HTTP2_STREAM_ABORTED' }));
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.aborted, true);
    assert.strictEqual(req.readableEnded, false);
  }));
  finished(req, common.mustCall((err) => {
    assert.ok(err);
  }));
}, codes.length));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  let pending = codes.length;

  for (const code of codes) {
    const req = client.request({
      ':method': 'POST',
      'content-length': '1000',
    });
    req.on('error', () => {});
    req.on('close', common.mustCall(() => {
      if (--pending === 0) {
        client.close();
        server.close();
      }
    }));
    // Send part of the body, then reset the stream without END_STREAM once
    // the server has seen it.
    req.write('{"action":"partial"}');
    req.on('response', common.mustCall(() => req.close(code)));
  }
}));

// A complete body followed by a reset still ends the request normally.
{
  const server = http2.createServer(common.mustCall((req, res) => {
    let body = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => body += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(body, 'complete');
      res.writeHead(200).flushHeaders();
    }));
    req.on('error', common.mustNotCall());
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({ ':method': 'POST' });
    req.on('error', () => {});
    req.on('response', common.mustCall(() => {
      req.close(http2.constants.NGHTTP2_CANCEL);
    }));
    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
    req.end('complete');
  }));
}
