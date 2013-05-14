// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
