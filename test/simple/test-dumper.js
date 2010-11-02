var assert =require('assert');
var IOWatcher = process.binding('io_watcher').IOWatcher;
var errnoException = process.binding('net').errnoException;
var close = process.binding('net').close;
var net = require('net');

var ncomplete = 0;





function test (N, b, cb) {
  //console.trace();
  var expected = N * b.length;
  var nread = 0;

  // Create a pipe
  var fds = process.binding('net').pipe();
  console.log("fds == %j", fds);

  // Use writev/dumper to send data down the write end of the pipe, fds[1].
  // This requires a IOWatcher.
  var w = new IOWatcher();
  w.set(fds[1], false, true);

  w.callback = function (readable, writable) {
    assert.ok(!readable && writable); // not really important.
    // Insert watcher into dumpQueue
    w.next = IOWatcher.dumpQueue.next;
    IOWatcher.dumpQueue.next = w;
  }

  var ndrain = 0;
  w.ondrain = function () {
    ndrain++;
  }

  var nerror = 0;
  w.onerror = function (errno) {
    throw errnoException(errno);
    nerror++;
  }

  // The read end, fds[0], will be used to count how much comes through.
  // This sets up a readable stream on fds[0].
  var stream = new net.Stream();
  stream.open(fds[0]);
  stream.readable = true;
  stream.resume();


  // Count the data as it arrives on the read end of the pipe.
  stream.on('data', function (d) {
    nread += d.length;

    if (nread >= expected) {
      assert.ok(nread === expected);
      assert.equal(1, ndrain);
      assert.equal(0, nerror);
      console.error("done. wrote %d bytes\n", nread);
      close(fds[1]);
    }
  });

  stream.on('close', function () {
    // check to make sure the watcher isn't in the dump queue.
    for (var x = IOWatcher.dumpQueue; x; x = x.next) {
      assert.ok(x !== w);
    }
    assert.equal(null, w.next);
    // completely flushed
    assert.ok(!w.firstBucket);
    assert.ok(!w.lastBucket);

    ncomplete++;
    if (cb) cb();
  });


  // Insert watcher into dumpQueue
  w.next = IOWatcher.dumpQueue.next;
  IOWatcher.dumpQueue.next = w;

  w.firstBucket = { data: b };
  w.lastBucket = w.firstBucket;

  for (var i = 0; i < N-1; i++) {
    var bucket = { data: b };
    assert.ok(!w.lastBucket.next);
    w.lastBucket.next = bucket;
    w.lastBucket = bucket;
  }
}


function runTests (values) {
  expectedToComplete = values.length;

  function go () {
    if (ncomplete < values.length) {
      var v = values[ncomplete];
      console.log("test N=%d, size=%d", v[0], v[1].length);
      test(v[0], v[1], go);
    }
  }

  go();
}

runTests([ [3, Buffer(1000)],
           [30, Buffer(1000)],
           [4, Buffer(10000)],
           [1, "hello world\n"],
           [50, Buffer(1024*1024)],
           [500, Buffer(40960+1)],
           [500, Buffer(40960-1)],
           [500, Buffer(40960)],
           [500, Buffer(1024*1024+1)],
           [50000, "hello world\n"]
         ]);


process.on('exit', function () {
  assert.equal(expectedToComplete, ncomplete);
});

