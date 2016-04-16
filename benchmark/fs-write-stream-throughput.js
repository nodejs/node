// If there are no args, then this is the root.  Run all the benchmarks!
'use strict';
if (!process.argv[2])
  parent();
else
  runTest(+process.argv[2], +process.argv[3], process.argv[4]);

function parent() {
  var types = [ 'string', 'buffer' ];
  var durs = [ 1, 5 ];
  var sizes = [ 1, 10, 100, 2048, 10240 ];
  var queue = [];
  types.forEach(function(t) {
    durs.forEach(function(d) {
      sizes.forEach(function(s) {
        queue.push([__filename, d, s, t]);
      });
    });
  });

  var spawn = require('child_process').spawn;
  var node = process.execPath;

  run();

  function run() {
    var args = queue.shift();
    if (!args)
      return;
    var child = spawn(node, args, { stdio: 'inherit' });
    child.on('close', function(code, signal) {
      if (code)
        throw new Error('Benchmark failed: ' + args.slice(1));
      run();
    });
  }
}

function runTest(dur, size, type) {
  if (type !== 'string')
    type = 'buffer';
  var chunk;
  switch (type) {
    case 'string':
      chunk = new Array(size + 1).join('a');
      break;
    case 'buffer':
      chunk = Buffer.alloc(size, 'a');
      break;
  }

  var fs = require('fs');
  try { fs.unlinkSync('write_stream_throughput'); } catch (e) {}

  var start;
  var end;
  function done() {
    var time = end[0] + end[1] / 1E9;
    var written = fs.statSync('write_stream_throughput').size / 1024;
    var rate = (written / time).toFixed(2);
    console.log('fs_write_stream_dur_%d_size_%d_type_%s: %d',
                dur, size, type, rate);

    try { fs.unlinkSync('write_stream_throughput'); } catch (e) {}
  }

  var f = require('fs').createWriteStream('write_stream_throughput');
  f.on('drain', write);
  f.on('open', write);
  f.on('close', done);

  // streams2 fs.WriteStreams will let you send a lot of writes into the
  // buffer before returning false, so capture the *actual* end time when
  // all the bytes have been written to the disk, indicated by 'finish'
  f.on('finish', function() {
    end = process.hrtime(start);
  });

  var ending = false;
  function write() {
    // don't try to write after we end, even if a 'drain' event comes.
    // v0.8 streams are so sloppy!
    if (ending)
      return;

    start = start || process.hrtime();
    while (false !== f.write(chunk));
    end = process.hrtime(start);

    if (end[0] >= dur) {
      ending = true;
      f.end();
    }
  }
}
