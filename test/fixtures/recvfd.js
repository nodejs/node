// See test/disabled/test-sendfd.js for a complete description of what this
// script is doing and how it fits into the test as a whole.

const net = require('net');

const receivedData = [];
const receivedFDs = [];
const numSentMessages = 0;

function processData(s) {
  if (receivedData.length == 0 || receivedFDs.length == 0) {
    return;
  }

  const fd = receivedFDs.shift();
  const d = receivedData.shift();

  // Augment our received object before sending it back across the pipe.
  d.pid = process.pid;

  // Create a stream around the FD that we received and send a serialized
  // version of our modified object back. Clean up when we're done.
  const pipeStream = new net.Stream(fd);

  const drainFunc = () => {
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
const s = new net.Stream();
s.on('fd', (fd) => {
  receivedFDs.unshift(fd);
  processData(s);
});
s.on('data', (data) => {
  data.toString('utf8').trim().split('\n').forEach((d) => {
    receivedData.unshift(JSON.parse(d));
  });
  processData(s);
});
s.connect(process.argv[2]);

// vim:ts=2 sw=2 et
