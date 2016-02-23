/* eslint-disable strict */
var common = require('../common');
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
  connection.setEncoding('binary');
  connection.on('data', function(chunk) {
    connection.write(chunk, 'binary');
  });
  connection.on('end', function() {
    connection.end();
  });
});
echoServer.listen(common.PORT);

var recv = '';

echoServer.on('listening', function() {
  var j = 0;
  var c = net.createConnection({
    port: common.PORT
  });

  c.setEncoding('binary');
  c.on('data', function(chunk) {
    var n = j + chunk.length;
    while (j < n && j < 256) {
      c.write(String.fromCharCode(j), 'binary');
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
