'use strict';
require('../common');
var http = require('http');
var assert = require('assert');

// first 204 or 304 works, subsequent anything fails
var codes = [204, 200];

// Methods don't really matter, but we put in something realistic.
var methods = ['DELETE', 'DELETE'];

var server = http.createServer(function(req, res) {
  var code = codes.shift();
  assert.equal('number', typeof code);
  assert.ok(code > 0);
  console.error('writing %d response', code);
  res.writeHead(code, {});
  res.end();
});

function nextRequest() {
  var method = methods.shift();
  console.error('writing request: %s', method);

  var request = http.request({
    port: server.address().port,
    method: method,
    path: '/'
  }, function(response) {
    response.on('end', function() {
      if (methods.length === 0) {
        console.error('close server');
        server.close();
      } else {
        // throws error:
        nextRequest();
        // works just fine:
        //process.nextTick(nextRequest);
      }
    });
    response.resume();
  });
  request.end();
}

server.listen(0, nextRequest);
