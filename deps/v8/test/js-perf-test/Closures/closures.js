// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Closures', [1000], [
  new Benchmark('ShortLivingClosures', false, false, 0,
    ShortLivingClosures, ShortLivingClosuresSetup, ShortLivingClosuresTearDown)
]);

// ----------------------------------------------------------------------------


// The pattern is this example is very common in Node.js.
var fs = {
  readFile: function(filename, cb) {
    cb(null, {length: 12});
  }
};


function printLength (filename) {
  fs.readFile(filename, foo);

  function foo (err, buf)  {
    if (err) return;
    for (var j = 0; j<1000; j++) {
      // Do some work to make the optimization actually worth while
      buf.length++;
    }
    return (buf.length);
  }
}


function ShortLivingClosuresSetup() {}

function ShortLivingClosures() {
  result = printLength('foo_bar.js');
}

function ShortLivingClosuresTearDown() {
  return result == 1012;
}
