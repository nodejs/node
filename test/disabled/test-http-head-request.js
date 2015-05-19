'use strict';
var common = require('../common');
var assert = require('assert');

var assert = require('assert');
var http = require('http');
var util = require('util');

var body = 'hello world';

var server = http.createServer(function(req, res) {
  res.writeHeader(200,
                  {'Content-Length': body.length.toString(),
                    'Content-Type': 'text/plain'
                  });
  console.log('method: ' + req.method);
  if (req.method != 'HEAD') res.write(body);
  res.end();
});
server.listen(common.PORT);

var gotEnd = false;

server.on('listening', function() {
  var request = http.request({
    port: common.PORT,
    method: 'HEAD',
    path: '/'
  }, function(response) {
    console.log('got response');
    response.on('data', function() {
      process.exit(2);
    });
    response.on('end', function() {
      process.exit(0);
    });
  });
  request.end();
});

//give a bit of time for the server to respond before we check it
setTimeout(function() {
  process.exit(1);
}, 2000);
