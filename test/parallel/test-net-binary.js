/* eslint-disable strict */
var common = require('../common');
var assert = require('assert');
var net = require('net');

var binaryString = '';
for (var i = 255; i >= 0; i--) {
  var s = '\'\\' + i.toString(8) + '\'';
  var S = eval(s);
  common.error(s +
               ' ' +
               JSON.stringify(S) +
               ' ' +
               JSON.stringify(String.fromCharCode(i)) +
               ' ' +
               S.charCodeAt(0));
  assert.ok(S.charCodeAt(0) == i);
  assert.ok(S == String.fromCharCode(i));
  binaryString += S;
}

// safe constructor
var echoServer = net.Server(function(connection) {
  console.error('SERVER got connection');
  connection.setEncoding('binary');
  connection.on('data', function(chunk) {
    common.error('SERVER recved: ' + JSON.stringify(chunk));
    connection.write(chunk, 'binary');
  });
  connection.on('end', function() {
    console.error('SERVER ending');
    connection.end();
  });
});
echoServer.listen(common.PORT);

var recv = '';

echoServer.on('listening', function() {
  console.error('SERVER listening');
  var j = 0;
  var c = net.createConnection({
    port: common.PORT
  });

  c.setEncoding('binary');
  c.on('data', function(chunk) {
    console.error('CLIENT data %j', chunk);
    var n = j + chunk.length;
    while (j < n && j < 256) {
      common.error('CLIENT write ' + j);
      c.write(String.fromCharCode(j), 'binary');
      j++;
    }
    if (j === 256) {
      console.error('CLIENT ending');
      c.end();
    }
    recv += chunk;
  });

  c.on('connect', function() {
    console.error('CLIENT connected, writing');
    c.write(binaryString, 'binary');
  });

  c.on('close', function() {
    console.error('CLIENT closed');
    console.dir(recv);
    echoServer.close();
  });

  c.on('finish', function() {
    console.error('CLIENT finished');
  });
});

process.on('exit', function() {
  console.log('recv: ' + JSON.stringify(recv));

  assert.equal(2 * 256, recv.length);

  var a = recv.split('');

  var first = a.slice(0, 256).reverse().join('');
  console.log('first: ' + JSON.stringify(first));

  var second = a.slice(256, 2 * 256).join('');
  console.log('second: ' + JSON.stringify(second));

  assert.equal(first, second);
});
