// Call fs.readFile over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.

var path = require('path');
var filename = path.resolve(__dirname, 'http.sh');
var fs = require('fs');
var count = 0;
var go = true;
var len = -1;
var assert = require('assert');

var concurrency = 1;
var encoding = null;
var time = 10;

for (var i = 2; i < process.argv.length; i++) {
  var arg = process.argv[i];
  if (arg.match(/^-e$/)) {
    encoding = process.argv[++i] || null;
  } else if (arg.match(/^-c$/)) {
    concurrency = ~~process.argv[++i];
    if (concurrency < 1) concurrency = 1;
  } else if (arg === '-t') {
    time = ~~process.argv[++i];
    if (time < 1) time = 1;
  }
}


setTimeout(function() {
  go = false;
}, time * 1000);

function round(n) {
  return Math.floor(n * 100) / 100;
}

var start = Date.now();
while (concurrency--) readFile();

function readFile() {
  if (!go) {
    process.stdout.write('\n');
    console.log('read the file %d times (higher is better)', count);
    var end = Date.now();
    var elapsed = (end - start) / 1000;
    var ns = elapsed * 1E9;
    var nsper = round(ns / count);
    console.log('%d ns per read (lower is better)', nsper);
    var readsper = round(count / (ns / 1E9));
    console.log('%d reads per sec (higher is better)', readsper);
    process.exit(0);
    return;
  }

  if (!(count % 1000)) {
    process.stdout.write('.');
  }

  if (encoding) fs.readFile(filename, encoding, then);
  else fs.readFile(filename, then);

  function then(er, data) {
    assert.ifError(er);
    count++;
    // basic sanity test: we should get the same number of bytes each time.
    if (count === 1) len = data.length;
    else assert(len === data.length);
    readFile();
  }
}
