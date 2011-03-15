// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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

  pipeStream.addListener('drain', drainFunc);
  pipeStream.resume();

  if (pipeStream.write(JSON.stringify(d) + '\n')) {
    drainFunc();
  }
}

// Create a UNIX socket to the path defined by argv[2] and read a file
// descriptor and misc data from it.
var s = new net.Stream();
s.addListener('fd', function(fd) {
  receivedFDs.unshift(fd);
  processData(s);
});
s.addListener('data', function(data) {
  data.toString('utf8').trim().split('\n').forEach(function(d) {
    receivedData.unshift(JSON.parse(d));
  });
  processData(s);
});
s.connect(process.argv[2]);

// vim:ts=2 sw=2 et
