// See test/simple/test-sendfd.js for a complete description of what this
// script is doing and how it fits into the test as a whole.

var net = require('net');

var receivedData = [];
var receivedFDs = [];
var numSentMessages = 0;

function processData(s) {
  if (receivedData.length == 0 || receivedFDs.length == 0) {
    return;
  }

  var fd = receivedFDs.shift();
  var d = receivedData.shift();

  // Augment our received object before sending it back across the pipe.
  d.pid = process.pid;

  // Create a stream around the FD that we received and send a serialized
  // version of our modified object back. Clean up when we're done.
  var pipeStream = new net.Stream(fd);

  var drainFunc = function() {
    pipeStream.destroy();

    if (++numSentMessages == 2) {
      s.destroy();
    }
  };

  pipeStream.on('drain', drainFunc);
  pipeStream.resume();

  if (pipeStream.write(JSON.stringify(d) + '\n')) {
    drainFunc();
  }
}

// Create a UNIX socket to the path defined by argv[2] and read a file
// descriptor and misc data from it.
var s = new net.Stream();
s.on('fd', function(fd) {
  receivedFDs.unshift(fd);
  processData(s);
});
s.on('data', function(data) {
  data.toString('utf8').trim().split('\n').forEach(function(d) {
    receivedData.unshift(JSON.parse(d));
  });
  processData(s);
});
s.connect(process.argv[2]);

// vim:ts=2 sw=2 et
