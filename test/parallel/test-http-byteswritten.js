'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const body = 'hello world\n';

const httpServer = http.createServer(common.mustCall(function(req, res) {
  httpServer.close();

  res.on('finish', common.mustCall(function() {
    assert.strictEqual(typeof req.connection.bytesWritten, 'number');
    assert(req.connection.bytesWritten > 0);
  }));
  res.writeHead(200, { 'Content-Type': 'text/plain' });

  // Write 1.5mb to cause some requests to buffer
  // Also, mix up the encodings a bit.
  const chunk = '7'.repeat(1024);
  const bchunk = Buffer.from(chunk);
  for (let i = 0; i < 1024; i++) {
    res.write(chunk);
    res.write(bchunk);
    res.write(chunk, 'hex');
  }
  // Get .bytesWritten while buffer is not empty
  assert(res.connection.bytesWritten > 0);

  res.end(body);
}));

httpServer.listen(0, function() {
  http.get({ port: this.address().port });
});
