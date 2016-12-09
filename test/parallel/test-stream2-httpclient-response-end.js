'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');
var msg = 'Hello';
var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end(msg);
}).listen(0, function() {
  http.get({port: this.address().port}, function(res) {
    var data = '';
    res.on('readable', common.mustCall(function() {
      console.log('readable event');
      data += res.read();
    }));
    res.on('end', common.mustCall(function() {
      console.log('end event');
      assert.strictEqual(msg, data);
      server.close();
    }));
  });
});
