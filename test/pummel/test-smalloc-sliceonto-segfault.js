'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
var alloc = process.binding('smalloc').alloc;
var sliceOnto = process.binding('smalloc').sliceOnto;


// child
if (process.argv[2] === 'child') {

  // test that many slices won't cause early ones to free prematurely
  // note: if this fails then it will cause v8 to seg fault
  var arr = [];
  for (var i = 0; i < 1e4; i++) {
    var a = alloc({}, 256);
    var b = {};
    b._data = sliceOnto(a, b, 0, 1);
    if (i % 2 === 0)
      arr.push(b);
    if (i % 10 === 0) gc();
  }
  var sum = 0;
  for (var i = 0; i < 5e3; i++) {
    sum += arr[i][0];
    arr[i][0] = sum;
  }

} else {
  // test case
  var child = spawn(process.execPath,
                ['--expose_gc', __filename, 'child']);

  child.on('exit', function(code, signal) {
    assert.equal(code, 0, signal);
    console.log('sliceOnto didn\'t segfault');
  });
}
