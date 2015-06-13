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
  common.error('pump!');
  util.pump(fs.createReadStream(fn), stream, function() {
    common.error('server stream close');
    common.error('server close');
    server.close();
  });
});

server.listen(common.PORT, function() {
  var conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');
  conn.on('data', function(chunk) {
    common.error('recv data! nchars = ' + chunk.length);
    buffer += chunk;
  });

  conn.on('end', function() {
    conn.end();
  });
  conn.on('close', function() {
    common.error('client connection close');
  });
});

var buffer = '';
var count = 0;

server.on('listening', function() {
});

process.on('exit', function() {
  assert.equal(expected, buffer);
});
