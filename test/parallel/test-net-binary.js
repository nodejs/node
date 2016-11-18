/* eslint-disable strict */
require('../common');
const assert = require('assert');
const net = require('net');

var binaryString = '';
for (var i = 255; i >= 0; i--) {
  const s = `'\\${i.toString(8)}'`;
  const S = eval(s);
  assert.strictEqual(S.charCodeAt(0), i);
  assert.strictEqual(S, String.fromCharCode(i));
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
  const c = net.createConnection({
    port: this.address().port
  });

  c.setEncoding('latin1');
  c.on('data', function(chunk) {
    const n = j + chunk.length;
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

  const a = recv.split('');

  const first = a.slice(0, 256).reverse().join('');

  const second = a.slice(256, 2 * 256).join('');

  assert.strictEqual(first, second);
});
