var common = require('../common');
var assert = require('assert');

var http = require('http');
var net = require('net');

// Create a TCP server
var srv = net.createServer(function(c) {
  c.write('bad http - should trigger parse error\r\n');

  console.log('connection');

  c.addListener('end', function() { c.end(); });
});

var parseError = false;

srv.listen(common.PORT, '127.0.0.1', function() {
  var hc = http.createClient(common.PORT, '127.0.0.1');
  hc.request('GET', '/').end();

  hc.on('error', function(e) {
    console.log('got error from client');
    srv.close();
    assert.ok(e.message.indexOf('Parse Error') >= 0);
    parseError = true;
  });
});


process.addListener('exit', function() {
  assert.ok(parseError);
});


