'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

var port = common.PORT;

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('OK');
});

var agent = new http.Agent({maxSockets: 1});

server.listen(port, function() {

  for (var i = 0; i < 11; ++i) {
    createRequest().end();
  }

  function callback() {}

  var count = 0;

  function createRequest() {
    var req = http.request({port: port, path: '/', agent: agent},
                           function(res) {
      req.clearTimeout(callback);

      res.on('end', function() {
        count++;

        if (count == 11) {
          server.close();
        }
      });

      res.resume();
    });

    req.setTimeout(1000, callback);
    return req;
  }
});
