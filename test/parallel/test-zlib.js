var common = require('../common');
var assert = require('assert');
var zlib = require('zlib');
var path = require('path');

var zlibPairs =
    [[zlib.Deflate, zlib.Inflate],
     [zlib.Gzip, zlib.Gunzip],
     [zlib.Deflate, zlib.Unzip],
     [zlib.Gzip, zlib.Unzip],
     [zlib.DeflateRaw, zlib.InflateRaw]];

// how fast to trickle through the slowstream
var trickle = [128, 1024, 1024 * 1024];

// tunable options for zlib classes.

// several different chunk sizes
var chunkSize = [128, 1024, 1024 * 16, 1024 * 1024];

// this is every possible value.
var level = [-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
var windowBits = [8, 9, 10, 11, 12, 13, 14, 15];
var memLevel = [1, 2, 3, 4, 5, 6, 7, 8, 9];
var strategy = [0, 1, 2, 3, 4];

// it's nice in theory to test every combination, but it
// takes WAY too long.  Maybe a pummel test could do this?
if (!process.env.PUMMEL) {
  trickle = [1024];
  chunkSize = [1024 * 16];
  level = [6];
  memLevel = [8];
  windowBits = [15];
  strategy = [0];
}

var fs = require('fs');

var testFiles = ['person.jpg', 'elipses.txt', 'empty.txt'];

if (process.env.FAST) {
  zlibPairs = [[zlib.Gzip, zlib.Unzip]];
  var testFiles = ['person.jpg'];
}

var tests = {};
testFiles.forEach(function(file) {
  tests[file] = fs.readFileSync(path.resolve(common.fixturesDir, file));
});

var util = require('util');
var stream = require('stream');


// stream that saves everything
function BufferStream() {
  this.chunks = [];
  this.length = 0;
  this.writable = true;
  this.readable = true;
}

util.inherits(BufferStream, stream.Stream);

BufferStream.prototype.write = function(c) {
  this.chunks.push(c);
  this.length += c.length;
  return true;
};

BufferStream.prototype.end = function(c) {
  if (c) this.write(c);
  // flatten
  var buf = new Buffer(this.length);
  var i = 0;
  this.chunks.forEach(function(c) {
    c.copy(buf, i);
    i += c.length;
  });
  this.emit('data', buf);
  this.emit('end');
  return true;
};


function SlowStream(trickle) {
  this.trickle = trickle;
  this.offset = 0;
  this.readable = this.writable = true;
}

util.inherits(SlowStream, stream.Stream);

SlowStream.prototype.write = function() {
  throw new Error('not implemented, just call ss.end(chunk)');
};

SlowStream.prototype.pause = function() {
  this.paused = true;
  this.emit('pause');
};

SlowStream.prototype.resume = function() {
  var self = this;
  if (self.ended) return;
  self.emit('resume');
  if (!self.chunk) return;
  self.paused = false;
  emit();
  function emit() {
    if (self.paused) return;
    if (self.offset >= self.length) {
      self.ended = true;
      return self.emit('end');
    }
    var end = Math.min(self.offset + self.trickle, self.length);
    var c = self.chunk.slice(self.offset, end);
    self.offset += c.length;
    self.emit('data', c);
    process.nextTick(emit);
  }
};

SlowStream.prototype.end = function(chunk) {
  // walk over the chunk in blocks.
  var self = this;
  self.chunk = chunk;
  self.length = chunk.length;
  self.resume();
  return self.ended;
};



// for each of the files, make sure that compressing and
// decompressing results in the same data, for every combination
// of the options set above.
var failures = 0;
var total = 0;
var done = 0;

Object.keys(tests).forEach(function(file) {
  var test = tests[file];
  chunkSize.forEach(function(chunkSize) {
    trickle.forEach(function(trickle) {
      windowBits.forEach(function(windowBits) {
        level.forEach(function(level) {
          memLevel.forEach(function(memLevel) {
            strategy.forEach(function(strategy) {
              zlibPairs.forEach(function(pair) {
                var Def = pair[0];
                var Inf = pair[1];
                var opts = { level: level,
                  windowBits: windowBits,
                  memLevel: memLevel,
                  strategy: strategy };

                total++;

                var def = new Def(opts);
                var inf = new Inf(opts);
                var ss = new SlowStream(trickle);
                var buf = new BufferStream();

                // verify that the same exact buffer comes out the other end.
                buf.on('data', function(c) {
                  var msg = file + ' ' +
                      chunkSize + ' ' +
                      JSON.stringify(opts) + ' ' +
                      Def.name + ' -> ' + Inf.name;
                  var ok = true;
                  var testNum = ++done;
                  for (var i = 0; i < Math.max(c.length, test.length); i++) {
                    if (c[i] !== test[i]) {
                      ok = false;
                      failures++;
                      break;
                    }
                  }
                  if (ok) {
                    console.log('ok ' + (testNum) + ' ' + msg);
                  } else {
                    console.log('not ok ' + (testNum) + ' ' + msg);
                    console.log('  ...');
                    console.log('  testfile: ' + file);
                    console.log('  type: ' + Def.name + ' -> ' + Inf.name);
                    console.log('  position: ' + i);
                    console.log('  options: ' + JSON.stringify(opts));
                    console.log('  expect: ' + test[i]);
                    console.log('  actual: ' + c[i]);
                    console.log('  chunkSize: ' + chunkSize);
                    console.log('  ---');
                  }
                });

                // the magic happens here.
                ss.pipe(def).pipe(inf).pipe(buf);
                ss.end(test);
              });
            }); }); }); }); }); }); // sad stallman is sad.
});

process.on('exit', function(code) {
  console.log('1..' + done);
  assert.equal(done, total, (total - done) + ' tests left unfinished');
  assert.ok(!failures, 'some test failures');
});
