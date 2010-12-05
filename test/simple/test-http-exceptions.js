var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
  intentionally_not_defined();
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('Thank you, come again.');
  res.end();
});

server.listen(common.PORT, function() {
  var req;
  for (var i = 0; i < 4; i += 1) {
    req = http.createClient(common.PORT).request('GET', '/busy/' + i);
    req.end();
  }
});

var exception_count = 0;

process.addListener('uncaughtException', function(err) {
  console.log('Caught an exception: ' + err);
  if (err.name === 'AssertionError') throw err;
  if (++exception_count == 4) process.exit(0);
});

