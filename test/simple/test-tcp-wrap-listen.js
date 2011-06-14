var common = require('../common');
var assert = require('assert');

var TCP = process.binding('tcp_wrap').TCP;

var server = new TCP();

var r = server.bind("0.0.0.0", common.PORT);
assert.equal(0, r);

server.listen(128);

var slice, sliceCount = 0, eofCount = 0;

server.onconnection = function(client) {
  console.log("got connection");

  client.readStart();
  client.onread = function(s) {
    if (s) {
      slice = s;
      sliceCount++;
    } else {
      console.log("eof");
      client.close();
      server.close();
      eofCount++;
    }
  };
};



var net = require('net');

var c = net.createConnection(common.PORT);
c.on('connect', function() {
  c.end("hello world");
});

c.on('close', function() {
  console.error("client closed");
});

process.on('exit', function() {
  assert.equal(1, sliceCount);
  assert.equal(1, eofCount);
  assert.ok(slice);
  assert.ok(slice.slab);

  var s = slice.slab.slice(slice.offset, slice.length, "ascii");
  assert.equal("hello world", s);
});



