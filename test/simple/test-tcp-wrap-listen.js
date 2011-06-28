var common = require('../common');
var assert = require('assert');

var TCP = process.binding('tcp_wrap').TCP;

var server = new TCP();

var r = server.bind("0.0.0.0", common.PORT);
assert.equal(0, r);

server.listen(128);

var slice, sliceCount = 0, eofCount = 0;

var writeCount = 0;
var recvCount = 0;

server.onconnection = function(client) {
  assert.equal(0, client.writeQueueSize);
  console.log("got connection");

  function maybeCloseClient() {
    if (client.pendingWrites.length == 0 && client.gotEOF) {
      console.log("close client");
      client.close();
    }
  }

  client.readStart();
  client.pendingWrites = [];
  client.onread = function(buffer, offset, length) {
    if (buffer) {
      assert.ok(length > 0);

      assert.equal(0, client.writeQueueSize);

      var req = client.write(buffer, offset, length);
      client.pendingWrites.push(req);

      console.log("client.writeQueueSize: " + client.writeQueueSize);
      // 11 bytes should flush
      assert.equal(0, client.writeQueueSize);

      req.oncomplete = function(status, client_, req_, buffer_) {
        assert.equal(req, client.pendingWrites.shift());

        // Check parameters.
        assert.equal(0, status);
        assert.equal(client, client_);
        assert.equal(req, req_);
        assert.equal(buffer, buffer_);

        console.log("client.writeQueueSize: " + client.writeQueueSize);
        assert.equal(0, client.writeQueueSize);

        writeCount++;
        console.log("write " + writeCount);
        maybeCloseClient();
      };

      sliceCount++;
    } else {
      console.log("eof");
      client.gotEOF = true;
      server.close();
      eofCount++;
      maybeCloseClient();
    }
  };
};

var net = require('net');

var c = net.createConnection(common.PORT);
c.on('connect', function() {
  c.end("hello world");
});

c.setEncoding('utf8');
c.on('data', function(d) {
  assert.equal('hello world', d);
  recvCount++;
});

c.on('close', function() {
  console.error("client closed");
});

process.on('exit', function() {
  assert.equal(1, sliceCount);
  assert.equal(1, eofCount);
  assert.equal(1, writeCount);
  assert.equal(1, recvCount);
});


