'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

// Issue #23116
// nghttp2 keeps closed stream structures around in memory (couple of hundred
// bytes each) until a session is closed. It does this to maintain the priority
// tree. However, it limits the number of requests that can be made in a
// session before our memory tracking (correctly) kicks in.
// The fix is to tell nghttp2 to forget about closed streams. We don't make use
// of priority anyway.
// Without the fix, this test fails at ~40k requests with an exception:
// Error [ERR_HTTP2_STREAM_ERROR]: Stream closed with error code
// NGHTTP2_ENHANCE_YOUR_CALM

const http2 = require('http2');
const assert = require('assert');

const server = http2.createServer({ maxSessionMemory: 1 });

server.on('session', function(session) {
  session.on('stream', function(stream) {
    stream.on('end', common.mustCall(function() {
      this.respond({
        ':status': 200
      }, {
        endStream: true
      });
    }));
    stream.resume();
  });
});

server.listen(0, function() {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  function next(i) {
    if (i === 10000) {
      client.close();
      return server.close();
    }
    const stream = client.request({ ':method': 'POST' });
    stream.on('response', common.mustCall(function(headers) {
      assert.strictEqual(headers[':status'], 200);
      this.on('close', common.mustCall(() => next(i + 1)));
    }));
    stream.end();
  }

  next(0);
});
