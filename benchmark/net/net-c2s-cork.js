// test the speed of .pipe() with sockets
'use strict';

var common = require('../common.js');
var PORT = common.PORT;

var bench = common.createBenchmark(main, {
  len: [4, 8, 16, 32, 64, 128, 512, 1024],
  type: ['buf'],
  dur: [5],
});

var dur;
var len;
var type;
var chunk;
var encoding;

function main(conf) {
  dur = +conf.dur;
  len = +conf.len;
  type = conf.type;

  switch (type) {
    case 'buf':
      chunk = Buffer.alloc(len, 'x');
      break;
    case 'utf':
      encoding = 'utf8';
      chunk = new Array(len / 2 + 1).join('ü');
      break;
    case 'asc':
      encoding = 'ascii';
      chunk = new Array(len + 1).join('x');
      break;
    default:
      throw new Error('invalid type: ' + type);
  }

  server();
}

var net = require('net');

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

function server() {
  var writer = new Writer();

  // the actual benchmark.
  var server = net.createServer(function(socket) {
    socket.pipe(writer);
  });

  server.listen(PORT, function() {
    var socket = net.connect(PORT);
    socket.on('connect', function() {
      bench.start();

      socket.on('drain', send);
      send();

      setTimeout(function() {
        var bytes = writer.received;
        var gbits = (bytes * 8) / (1024 * 1024 * 1024);
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
