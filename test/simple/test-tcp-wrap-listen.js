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
  console.log("got connection");

  function maybeCloseClient() {
    if (client.pendingWrites.length == 0 && client.gotEOF) {
      console.log("close client");
      client.close();
    }
  }

  client.readStart();
  client.pendingWrites = [];
  client.onread = function(s) {
    if (s) {
      slice = s;
      var req = client.write(s.slab, s.offset, s.length);
      client.pendingWrites.push(req);
      req.oncomplete = function() {
        assert.equal(req, client.pendingWrites.shift());
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
  assert.ok(slice);
  assert.ok(slice.slab);

  var s = slice.slab.slice(slice.offset, slice.length, "ascii");
  assert.equal("hello world", s);
});


