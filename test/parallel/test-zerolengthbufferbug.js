'use strict';
// Serving up a zero-length buffer should work.

const common = require('../common');
var http = require('http');

var server = http.createServer(function(req, res) {
  var buffer = Buffer.alloc(0);
  // FIXME: WTF gjslint want this?
  res.writeHead(200, {'Content-Type': 'text/html',
                 'Content-Length': buffer.length});
  res.end(buffer);
});

server.listen(0, common.mustCall(function() {
  http.get({ port: this.address().port }, common.mustCall(function(res) {

    res.on('data', common.fail);

    res.on('end', function(d) {
      server.close();
    });
  }));
}));
