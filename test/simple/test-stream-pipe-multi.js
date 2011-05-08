// Test that having a bunch of streams piping in parallel
// doesn't break anything.

var common = require("../common");
var assert = require("assert");
var Stream = require("stream").Stream;
var rr = [];
var ww = [];
var cnt = 100;
var chunks = 1000;
var chunkSize = 250;
var data = new Buffer(chunkSize);
var wclosed = 0;
var rclosed = 0;

function FakeStream() {
  Stream.apply(this);
  this.wait = false;
  this.writable = true;
  this.readable = true;
}

FakeStream.prototype = Object.create(Stream.prototype);

FakeStream.prototype.write = function(chunk) {
  console.error(this.ID, "write", this.wait)
  if (this.wait) {
    process.nextTick(this.emit.bind(this, "drain"));
  }
  this.wait = !this.wait;
  return this.wait;
};

FakeStream.prototype.end = function() {
  this.emit("end");
  process.nextTick(this.close.bind(this));
};

// noop - closes happen automatically on end.
FakeStream.prototype.close = function() {
  this.emit("close");
};


// expect all streams to close properly.
process.on("exit", function() {
  assert.equal(cnt, wclosed, "writable streams closed");
  assert.equal(cnt, rclosed, "readable streams closed");
});

for (var i = 0; i < chunkSize; i ++) {
  chunkSize[i] = i % 256;
}

for (var i = 0; i < cnt; i++) {
  var r = new FakeStream();
  r.on("close", function() {
    console.error(this.ID, "read close");
    rclosed++;
  });
  rr.push(r);

  var w = new FakeStream();
  w.on("close", function() {
    console.error(this.ID, "write close");
    wclosed++;
  });
  ww.push(w);

  r.ID = w.ID = i;
  r.pipe(w);
}

// now start passing through data
// simulate a relatively fast async stream.
rr.forEach(function (r) {
  var cnt = chunks;
  var paused = false;

  r.on("pause", function() {
    paused = true;
  });

  r.on("resume", function() {
    paused = false;
    step();
  });

  function step() {
    r.emit("data", data);
    if (--cnt === 0) {
      r.end();
      return;
    }
    if (paused) return;
    process.nextTick(step);
  }

  process.nextTick(step);
});
