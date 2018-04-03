// test the speed of .pipe() with sockets
'use strict';

const common = require('../common.js');
const net = require('net');
const PORT = common.PORT;

const bench = common.createBenchmark(main, {
  len: [4, 8, 16, 32, 64, 128, 512, 1024],
  type: ['buf'],
  dur: [5],
});

var chunk;
var encoding;

function main({ dur, len, type }) {
  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(len, 'x');
      break;
    case 'utf':
      encoding = 'utf8';
      chunk = 'ü'.repeat(len / 2);
      break;
    case 'asc':
      encoding = 'ascii';
      chunk = 'x'.repeat(len);
      break;
    default:
      throw new Error(`invalid type: ${type}`);
  }

  const writer = new Writer();

  // the actual benchmark.
  const server = net.createServer(function(socket) {
    socket.pipe(writer);
  });

  server.listen(PORT, function() {
    const socket = net.connect(PORT);
    socket.on('connect', function() {
      bench.start();

      socket.on('drain', send);
      send();

      setTimeout(function() {
        const bytes = writer.received;
        const gbits = (bytes * 8) / (1024 * 1024 * 1024);
        bench.end(gbits);
        process.exit(0);
      }, dur * 1000);

      function send() {
        socket.cork();
        while (socket.write(chunk, encoding)) {}
        socket.uncork();
      }
    });
  });
}

function Writer() {
  this.received = 0;
  this.writable = true;
}

Writer.prototype.write = function(chunk, encoding, cb) {
  this.received += chunk.length;

  if (typeof encoding === 'function')
    encoding();
  else if (typeof cb === 'function')
    cb();

  return true;
};

// doesn't matter, never emits anything.
Writer.prototype.on = function() {};
Writer.prototype.once = function() {};
Writer.prototype.emit = function() {};
Writer.prototype.prependListener = function() {};
