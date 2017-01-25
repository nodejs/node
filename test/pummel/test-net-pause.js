'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const N = 200;
let recv = '', chars_recved = 0;

const server = net.createServer(function(connection) {
  function write(j) {
    if (j >= N) {
      connection.end();
      return;
    }
    setTimeout(function() {
      connection.write('C');
      write(j + 1);
    }, 10);
  }
  write(0);
});

server.on('listening', function() {
  const client = net.createConnection(common.PORT);
  client.setEncoding('ascii');
  client.on('data', function(d) {
    console.log(d);
    recv += d;
  });

  setTimeout(function() {
    chars_recved = recv.length;
    console.log('pause at: ' + chars_recved);
    assert.equal(true, chars_recved > 1);
    client.pause();
    setTimeout(function() {
      console.log('resume at: ' + chars_recved);
      assert.equal(chars_recved, recv.length);
      client.resume();

      setTimeout(function() {
        chars_recved = recv.length;
        console.log('pause at: ' + chars_recved);
        client.pause();

        setTimeout(function() {
          console.log('resume at: ' + chars_recved);
          assert.equal(chars_recved, recv.length);
          client.resume();

        }, 500);

      }, 500);

    }, 500);

  }, 500);

  client.on('end', function() {
    server.close();
    client.end();
  });
});
server.listen(common.PORT);

process.on('exit', function() {
  assert.equal(N, recv.length);
  console.error('Exit');
});
