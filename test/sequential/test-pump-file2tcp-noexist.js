'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var fs = require('fs');
var util = require('util');
var path = require('path');
var fn = path.join(common.fixturesDir, 'does_not_exist.txt');

var got_error = false;
var conn_closed = false;

var server = net.createServer(function(stream) {
  common.error('pump!');
  util.pump(fs.createReadStream(fn), stream, function(err) {
    common.error('util.pump\'s callback fired');
    if (err) {
      got_error = true;
    } else {
      console.error('util.pump\'s callback fired with no error');
      console.error('this shouldn\'t happen as the file doesn\'t exist...');
      assert.equal(true, false);
    }
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
    conn_closed = true;
  });
});

var buffer = '';

process.on('exit', function() {
  assert.equal(true, got_error);
  assert.equal(true, conn_closed);
  console.log('exiting');
});
