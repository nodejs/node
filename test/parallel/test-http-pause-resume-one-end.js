'use strict';
const common = require('../common');
var http = require('http');

var server = http.Server(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('Hello World\n');
  server.close();
});

server.listen(0, common.mustCall(function() {
  var opts = {
    port: this.address().port,
    headers: { connection: 'close' }
  };

  http.get(opts, common.mustCall(function(res) {
    res.on('data', common.mustCall(function(chunk) {
      res.pause();
      setTimeout(function() {
        res.resume();
      });
    }));

    res.on('end', common.mustCall(function() {}));
  }));
}));
