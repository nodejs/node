// test the speed of .pipe() with sockets

var net = require('net');
var start;

function Writer() {
  this.start = null;
  this.received = 0;
  this.writable = true;
  this.printStats = this.printStats.bind(this);
  this.interval = setInterval(this.printStats, 1000);
}

Writer.prototype.write = function(chunk, encoding, cb) {
  if (!this.start)
    this.start = process.hrtime();

  this.received += chunk.length;

  if (typeof encoding === 'function')
    encoding();
  else if (typeof cb === 'function')
    cb();

  return true;
};

// doesn't matter, never emits anything.
Writer.prototype.on = function() {};

Writer.prototype.emit = function() {};

var statCounter = 0;
Writer.prototype.printStats = function() {
  if (!this.start || !this.received)
    return;
  var elapsed = process.hrtime(this.start);
  elapsed = elapsed[0] * 1E9 + elapsed[1];
  var bits = this.received * 8;
  var gbits = bits / (1024 * 1024 * 1024);
  var rate = (gbits / elapsed * 1E9).toFixed(4);
  console.log('%s Gbits/sec (%d bits / %d ns)', rate, bits, elapsed);

  // reset to keep getting instant time.
  this.start = process.hrtime();
  this.received = 0;

  // 100 seconds is enough time to get a few GC runs and stabilize.
  if (statCounter++ === 100)
    process.exit(0);
};

var len = process.env.LENGTH || 16 * 1024 * 1024;
var chunk = new Buffer(len);
for (var i = 0; i < len; i++) {
  chunk[i] = i % 256;
}

function Reader() {
  this.flow = this.flow.bind(this);
  this.readable = true;
}

Reader.prototype.pipe = function(dest) {
  this.dest = dest;
  this.flow();
  return dest;
};

Reader.prototype.flow = function() {
  var dest = this.dest;
  var res = dest.write(chunk);
  if (!res)
    dest.once('drain', this.flow);
  else
    process.nextTick(this.flow);
};


var reader = new Reader();
var writer = new Writer();

// the actual benchmark.
var server = net.createServer(function(socket) {
  socket.pipe(socket);
});

server.listen(1337, function() {
  var socket = net.connect(1337);
  socket.on('connect', function() {
    reader.pipe(socket);
    socket.pipe(writer);
  });
});

