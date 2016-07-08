'use strict';
require('../common');
var http = require('http');

// Simple test of Node's HTTP Client choking on a response
// with a 'Content-Length: 0 ' response header.
// I.E. a space character after the 'Content-Length' throws an `error` event.


var s = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': '0 '});
  res.end();
});
s.listen(0, function() {

  var request = http.request({ port: this.address().port }, function(response) {
    console.log('STATUS: ' + response.statusCode);
    s.close();
    response.resume();
  });

  request.end();
});
