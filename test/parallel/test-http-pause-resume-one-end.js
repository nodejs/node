'use strict';
const common = require('../common');
const http = require('http');

const server = http.Server(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('Hello World\n');
  server.close();
});

server.listen(0, common.mustCall(function() {
  const opts = {
    port: this.address().port,
    headers: { connection: 'close' }
  };

  http.get(opts, common.mustCall(function(res) {
    res.on('data', common.mustCall(function() {
      res.pause();
      setImmediate(function() {
        res.resume();
      });
    }));

    res.on('end', common.mustCall(function() {}));
  }));
}));
