// test the speed of .pipe() with sockets

var net = require('net');
var N = parseInt(process.argv[2]) || 100;
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
Writer.prototype.once = function() {};
Writer.prototype.emit = function() {};

var rates = [];
var statCounter = 0;
Writer.prototype.printStats = function() {
  if (!this.start || !this.received)
    return;
  var elapsed = process.hrtime(this.start);
  elapsed = elapsed[0] * 1E9 + elapsed[1];
  var bits = this.received * 8;
  var gbits = bits / (1024 * 1024 * 1024);
  var rate = gbits / elapsed * 1E9;
  rates.push(rate);
  console.log('%s Gbits/sec (%d bits / %d ns)', rate.toFixed(4), bits, elapsed);

  // reset to keep getting instant time.
  this.start = process.hrtime();
  this.received = 0;

  if (++statCounter === N) {
    report();
    process.exit(0);
  }
};

function report() {
  rates.sort();
  var min = rates[0];
  var max = rates[rates.length - 1];
  var median = rates[rates.length >> 1];
  var avg = 0;
  rates.forEach(function(rate) { avg += rate });
  avg /= rates.length;
  console.error('min:%s avg:%s max:%s median:%s',
                min.toFixed(2),
                avg.toFixed(2),
                max.toFixed(2),
                median.toFixed(2));
}

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

