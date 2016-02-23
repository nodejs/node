'use strict';
var common = require('../common');
var assert = require('assert');

// Make sure that throwing in 'end' handler doesn't lock
// up the socket forever.
//
// This is NOT a good way to handle errors in general, but all
// the same, we should not be so brittle and easily broken.

var http = require('http');

var n = 0;
var server = http.createServer(function(req, res) {
  if (++n === 10) server.close();
  res.end('ok');
});

server.listen(common.PORT, function() {
  for (var i = 0; i < 10; i++) {
    var options = { port: common.PORT };

    var req = http.request(options, function(res) {
      res.resume();
      res.on('end', function() {
        throw new Error('gleep glorp');
      });
    });
    req.end();
  }
});

setTimeout(function() {
  process.removeListener('uncaughtException', catcher);
  throw new Error('Taking too long!');
}, common.platformTimeout(1000)).unref();

process.on('uncaughtException', catcher);
var errors = 0;
function catcher() {
  errors++;
}

process.on('exit', function() {
  assert.equal(errors, 10);
  console.log('ok');
});
