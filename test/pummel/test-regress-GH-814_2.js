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

// Flags: --expose_gc

var common = require('../common');

var fs = require('fs');
var testFileName = require('path').join(common.tmpDir, 'GH-814_test.txt');
var testFD = fs.openSync(testFileName, 'w');
console.error(testFileName + '\n');


var tailProc = require('child_process').spawn('tail', ['-f', testFileName]);
tailProc.stdout.on('data', tailCB);

function tailCB(data) {
  PASS = data.toString().indexOf('.') < 0;

  if (PASS) {
    //console.error('i');
  } else {
    console.error('[FAIL]\n DATA -> ');
    console.error(data);
    console.error('\n');
    throw Error('Buffers GC test -> FAIL');
  }
}


var PASS = true;
var bufPool = [];
var kBufSize = 16 * 1024 * 1024;
var neverWrittenBuffer = newBuffer(kBufSize, 0x2e); //0x2e === '.'

var timeToQuit = Date.now() + 5e3; //Test should last no more than this.
writer();

function writer() {

  if (PASS) {
    if (Date.now() > timeToQuit) {
      setTimeout(function() {
        process.kill(tailProc.pid);
        console.error('\nBuffers GC test -> PASS (OK)\n');
      }, 555);
    } else {
      fs.write(testFD, newBuffer(kBufSize, 0x61), 0, kBufSize, -1, writerCB);
      gc();
      gc();
      gc();
      gc();
      gc();
      gc();
      var nuBuf = new Buffer(kBufSize);
      neverWrittenBuffer.copy(nuBuf);
      if (bufPool.push(nuBuf) > 100) {
        bufPool.length = 0;
      }
      process.nextTick(writer);
      //console.error('o');
    }
  }

}

function writerCB(err, written) {
  //console.error('cb.');
  if (err) {
    throw err;
  }
}




// ******************* UTILITIES


function newBuffer(size, value) {
  var buffer = new Buffer(size);
  while (size--) {
    buffer[size] = value;
  }
  buffer[buffer.length - 1] = 0x0d;
  buffer[buffer.length - 1] = 0x0a;
  return buffer;
}
