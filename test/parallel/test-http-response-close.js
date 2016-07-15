'use strict';
const common = require('../common');
var http = require('http');

var server = http.createServer(common.mustCall(function(req, res) {
  res.writeHead(200);
  res.write('a');

  req.on('close', common.mustCall(function() {
    console.error('request aborted');
  }));
  res.on('close', common.mustCall(function() {
    console.error('response aborted');
  }));
}));
server.listen(0);

server.on('listening', function() {
  console.error('make req');
  http.get({
    port: this.address().port
  }, function(res) {
    console.error('got res');
    res.on('data', function(data) {
      console.error('destroy res');
      res.destroy();
      server.close();
    });
  });
});
