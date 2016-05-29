'use strict';
require('../common');
var http = require('http');

var server = http.createServer(function(req, res) {
  intentionally_not_defined(); // eslint-disable-line no-undef
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('Thank you, come again.');
  res.end();
});

server.listen(0, function() {
  for (var i = 0; i < 4; i += 1) {
    http.get({ port: this.address().port, path: '/busy/' + i });
  }
});

var exception_count = 0;

process.on('uncaughtException', function(err) {
  console.log('Caught an exception: ' + err);
  if (err.name === 'AssertionError') throw err;
  if (++exception_count == 4) process.exit(0);
});

