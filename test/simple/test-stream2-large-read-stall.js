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

var common = require('../common.js');
var assert = require('assert');

// If everything aligns so that you do a read(n) of exactly the
// remaining buffer, then make sure that 'end' still emits.

var READSIZE = 100;
var PUSHSIZE = 20;
var PUSHCOUNT = 1000;
var HWM = 50;

var Readable = require('stream').Readable;
var r = new Readable({
  highWaterMark: HWM
});
var rs = r._readableState;

r._read = push;

r.on('readable', function() {
  console.error('>> readable');
  do {
    console.error('  > read(%d)', READSIZE);
    var ret = r.read(READSIZE);
    console.error('  < %j (%d remain)', ret && ret.length, rs.length);
  } while (ret && ret.length === READSIZE);

  console.error('<< after read()',
                ret && ret.length,
                rs.needReadable,
                rs.length);
});

var endEmitted = false;
r.on('end', function() {
  endEmitted = true;
  console.error('end');
});

var pushes = 0;
function push() {
  if (pushes > PUSHCOUNT)
    return;

  if (pushes++ === PUSHCOUNT) {
    console.error('   push(EOF)');
    return r.push(null);
  }

  console.error('   push #%d', pushes);
  if (r.push(new Buffer(PUSHSIZE)))
    setTimeout(push);
}

// start the flow
var ret = r.read(0);

process.on('exit', function() {
  assert.equal(pushes, PUSHCOUNT + 1);
  assert(endEmitted);
});
