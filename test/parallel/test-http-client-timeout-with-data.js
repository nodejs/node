'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

var nchunks = 0;

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

const server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': '2'});
  res.write('*');
  server.once('timeout', common.mustCall(function() { res.end('*'); }));
});

server.listen(0, options.host, function() {
  options.port = this.address().port;
  const req = http.request(options, onresponse);
  req.end();

  function onresponse(res) {
    req.setTimeout(50, common.mustCall(function() {
      assert.strictEqual(nchunks, 1); // should have received the first chunk
      server.emit('timeout');
    }));

    res.on('data', common.mustCall(function(data) {
      assert.strictEqual('' + data, '*');
      nchunks++;
    }, 2));

    res.on('end', common.mustCall(function() {
      assert.strictEqual(nchunks, 2);
      server.close();
    }));
  }
});
