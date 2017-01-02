'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');

var body = 'hello world\n';

var httpServer = http.createServer(common.mustCall(function(req, res) {
  httpServer.close();

  res.on('finish', common.mustCall(function() {
    assert.strictEqual(typeof req.connection.bytesWritten, 'number');
    assert(req.connection.bytesWritten > 0);
  }));
  res.writeHead(200, { 'Content-Type': 'text/plain' });

  // Write 1.5mb to cause some requests to buffer
  // Also, mix up the encodings a bit.
  var chunk = new Array(1024 + 1).join('7');
  var bchunk = Buffer.from(chunk);
  for (var i = 0; i < 1024; i++) {
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
