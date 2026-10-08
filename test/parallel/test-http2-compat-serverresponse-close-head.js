'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// A HEAD response must receive a close event when its stream closes before
// the response was sent, like a response to any other method. A headers-only
// HEAD response still waits for response.end() before finish and close.

{
  // The client cancels the stream before the server responds.
  let request;
  const server = h2.createServer(common.mustCall((req, res) => {
    res.on('finish', common.mustNotCall());
    res.on('close', common.mustCall(() => {
      // Ending an already closed response still calls back.
      res.end(common.mustCall());
      server.close();
    }));
    request.close(h2.constants.NGHTTP2_CANCEL);
  }));

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    request = client.request({ ':method': 'HEAD' });
    request.on('close', common.mustCall(() => client.close()));
  }));
}

{
  // The connection is lost before the server responds.
  let client;
  const server = h2.createServer(common.mustCall((req, res) => {
    res.on('finish', common.mustNotCall());
    res.on('close', common.mustCall(() => server.close()));
    client.destroy();
  }));

  server.listen(0, common.mustCall(() => {
    client = h2.connect(`http://localhost:${server.address().port}`);
    client.on('error', () => {});
    client.request({ ':method': 'HEAD' }).on('error', () => {});
  }));
}

{
  // The stream of a headers-only response closes before response.end().
  const server = h2.createServer(common.mustCall((req, res) => {
    let ended = false;
    res.on('finish', common.mustCall(() => assert(ended)));
    res.on('close', common.mustCall(() => {
      assert(ended);
      server.close();
    }));
    req.stream.on('close', common.mustCall(() => {
      setImmediate(() => {
        ended = true;
        res.end();
      });
    }));
    res.writeHead(200);
  }));

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    const request = client.request({ ':method': 'HEAD' });
    request.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
    }));
    request.resume();
    request.on('close', common.mustCall(() => client.close()));
  }));
}
