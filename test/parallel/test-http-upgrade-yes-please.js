'use strict';

var assert = require('assert');
var http = require('http');
var net = require('net');

var upgrades = 0;
process.on('exit', function() { assert.equal(upgrades, 1); });

http.createServer(assert.fail).listen(0, '127.0.0.1', function() {
  this.on('upgrade', function(req, conn, head) {
    conn.destroy();
    this.close();
    upgrades += 1;
  });
  var options = { host: this.address().address, port: this.address().port };
  net.connect(options, function() {
    this.write('GET / HTTP/1.1\r\n' +
               'Upgrade: Yes, please.\r\n' +
               '\r\n');
  });
});
