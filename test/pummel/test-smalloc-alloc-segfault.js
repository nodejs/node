'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
var alloc = process.binding('smalloc').alloc;


// child
if (process.argv[2] === 'child') {

  // test that many allocations won't cause early ones to free prematurely
  // note: if memory frees prematurely then reading out the values will cause
  // it to seg fault
  var arr = [];
  for (var i = 0; i < 1e4; i++) {
    arr.push(alloc({}, 1));
    alloc({}, 1024);
    if (i % 10 === 0) gc();
  }
  var sum = 0;
  for (var i = 0; i < 1e4; i++) {
    sum += arr[i][0];
    arr[i][0] = sum;
  }

} else {
  // test case
  var child = spawn(process.execPath,
                ['--expose_gc', __filename, 'child']);

  child.on('exit', function(code, signal) {
    assert.equal(code, 0, signal);
    console.log('alloc didn\'t segfault');
  });
}
