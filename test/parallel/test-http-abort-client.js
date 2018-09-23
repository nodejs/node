'use strict';
var common = require('../common');
var http = require('http');
var assert = require('assert');

var server = http.Server(function(req, res) {
  console.log('Server accepted request.');
  res.writeHead(200);
  res.write('Part of my res.');

  res.destroy();
});

var responseClose = false;

server.listen(common.PORT, function() {
  var client = http.get({
    port: common.PORT,
    headers: { connection: 'keep-alive' }

  }, function(res) {
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
    res.on('close', function() {
      console.log('Response aborted');
      responseClose = true;
    });
  });
});


process.on('exit', function() {
  assert.ok(responseClose);
});
