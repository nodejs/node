'use strict';
require('../common');
const http = require('http');

// Simple test of Node's HTTP Client choking on a response
// with a 'Content-Length: 0 ' response header.
// I.E. a space character after the 'Content-Length' throws an `error` event.


const s = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length': '0 '});
  res.end();
});
s.listen(0, function() {

  const request =
      http.request({ port: this.address().port }, function(response) {
        console.log('STATUS: ' + response.statusCode);
        s.close();
        response.resume();
      });

  request.end();
});
