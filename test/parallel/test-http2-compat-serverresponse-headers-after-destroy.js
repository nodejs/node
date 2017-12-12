'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// makes sure that Http2ServerResponse setHeader & removeHeader, do not throw
// any errors if the stream was destroyed before headers were sent

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    response.on('finish', common.mustCall(() => {
      assert.strictEqual(response.headersSent, false);
      assert.doesNotThrow(() => response.setHeader('test', 'value'));
      assert.doesNotThrow(() => response.removeHeader('test', 'value'));

      process.nextTick(() => {
        assert.doesNotThrow(() => response.setHeader('test', 'value'));
        assert.doesNotThrow(() => response.removeHeader('test', 'value'));

        server.close();
      });
    }));

    response.destroy();
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('end', common.mustCall(function() {
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
