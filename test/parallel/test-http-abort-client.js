'use strict';
const common = require('../common');
var http = require('http');

var server = http.Server(function(req, res) {
  console.log('Server accepted request.');
  res.writeHead(200);
  res.write('Part of my res.');

  res.destroy();
});

server.listen(0, common.mustCall(function() {
  http.get({
    port: this.address().port,
    headers: { connection: 'keep-alive' }
  }, common.mustCall(function(res) {
    server.close();

    console.log('Got res: ' + res.statusCode);
    console.dir(res.headers);

    res.on('data', function(chunk) {
      console.log('Read ' + chunk.length + ' bytes');
      console.log(' chunk=%j', chunk.toString());
    });

    res.on('end', function() {
      console.log('Response ended.');
    });

    res.on('aborted', function() {
      console.log('Response aborted.');
    });

    res.socket.on('close', function() {
      console.log('socket closed, but not res');
    });

    // it would be nice if this worked:
    res.on('close', common.mustCall(function() {}));
  }));
}));
