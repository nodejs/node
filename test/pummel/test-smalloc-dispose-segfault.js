'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
var smalloc = process.binding('smalloc');
var alloc = smalloc.alloc;
var dispose = smalloc.dispose;


// child
if (process.argv[2] === 'child') {

  // test that disposing an allocation won't cause the MakeWeakCallback to try
  // and free invalid memory
  for (var i = 0; i < 1e4; i++) {
    dispose(alloc({}, 5));
    if (i % 10 === 0) gc();
  }

} else {
  // test case
  var child = spawn(process.execPath,
                ['--expose_gc', __filename, 'child']);

  child.on('exit', function(code, signal) {
    assert.equal(code, 0, signal);
    console.log('dispose didn\'t segfault');
  });
}
