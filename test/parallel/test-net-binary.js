/* eslint-disable strict */
require('../common');
var assert = require('assert');
var net = require('net');

var binaryString = '';
for (var i = 255; i >= 0; i--) {
  var s = '\'\\' + i.toString(8) + '\'';
  var S = eval(s);
  assert.ok(S.charCodeAt(0) == i);
  assert.ok(S == String.fromCharCode(i));
  binaryString += S;
}

// safe constructor
var echoServer = net.Server(function(connection) {
  connection.setEncoding('latin1');
  connection.on('data', function(chunk) {
    connection.write(chunk, 'latin1');
  });
  connection.on('end', function() {
    connection.end();
  });
});
echoServer.listen(0);

var recv = '';

echoServer.on('listening', function() {
  var j = 0;
  var c = net.createConnection({
    port: this.address().port
  });

  c.setEncoding('latin1');
  c.on('data', function(chunk) {
    var n = j + chunk.length;
    while (j < n && j < 256) {
      c.write(String.fromCharCode(j), 'latin1');
      j++;
    }
    if (j === 256) {
      c.end();
    }
    recv += chunk;
  });

  c.on('connect', function() {
    c.write(binaryString, 'binary');
  });

  c.on('close', function() {
    echoServer.close();
  });
});

process.on('exit', function() {
  assert.equal(2 * 256, recv.length);

  var a = recv.split('');

  var first = a.slice(0, 256).reverse().join('');

  var second = a.slice(256, 2 * 256).join('');

  assert.equal(first, second);
});
