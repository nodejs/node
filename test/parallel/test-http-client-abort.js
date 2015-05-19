'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var clientAborts = 0;

var server = http.Server(function(req, res) {
  console.log('Got connection');
  res.writeHead(200);
  res.write('Working on it...');

  // I would expect an error event from req or res that the client aborted
  // before completing the HTTP request / response cycle, or maybe a new
  // event like "aborted" or something.
  req.on('aborted', function() {
    clientAborts++;
    console.log('Got abort ' + clientAborts);
    if (clientAborts === N) {
      console.log('All aborts detected, you win.');
      server.close();
    }
  });

  // since there is already clientError, maybe that would be appropriate,
  // since "error" is magical
  req.on('clientError', function() {
    console.log('Got clientError');
  });
});

var responses = 0;
var N = 16;
var requests = [];

server.listen(common.PORT, function() {
  console.log('Server listening.');

  for (var i = 0; i < N; i++) {
    console.log('Making client ' + i);
    var options = { port: common.PORT, path: '/?id=' + i };
    var req = http.get(options, function(res) {
      console.log('Client response code ' + res.statusCode);

      res.resume();
      if (++responses == N) {
        console.log('All clients connected, destroying.');
        requests.forEach(function(outReq) {
          console.log('abort');
          outReq.abort();
        });
      }
    });

    requests.push(req);
  }
});

process.on('exit', function() {
  assert.equal(N, clientAborts);
});
