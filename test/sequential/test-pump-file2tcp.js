'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var fs = require('fs');
var util = require('util');
var path = require('path');
var fn = path.join(common.fixturesDir, 'elipses.txt');

var expected = fs.readFileSync(fn, 'utf8');

var server = net.createServer(function(stream) {
  util.pump(fs.createReadStream(fn), stream, function() {
    server.close();
  });
});

server.listen(common.PORT, function() {
  var conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');
  conn.on('data', function(chunk) {
    buffer += chunk;
  });

  conn.on('end', function() {
    conn.end();
  });
});

var buffer = '';
var count = 0;

server.on('listening', function() {
});

process.on('exit', function() {
  assert.equal(expected, buffer);
});
