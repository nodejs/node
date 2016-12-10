'use strict';
require('../common');

var http = require('http');

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('OK');
});

var agent = new http.Agent({maxSockets: 1});

server.listen(0, function() {

  for (var i = 0; i < 11; ++i) {
    createRequest().end();
  }

  function callback() {}

  var count = 0;

  function createRequest() {
    const req = http.request(
      {port: server.address().port, path: '/', agent: agent},
      function(res) {
        req.clearTimeout(callback);

        res.on('end', function() {
          count++;

          if (count === 11) {
            server.close();
          }
        });

        res.resume();
      }
    );

    req.setTimeout(1000, callback);
    return req;
  }
});
