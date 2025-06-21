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

'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const f = tmpdir.resolve('x.txt');
fs.closeSync(fs.openSync(f, 'w'));

let changes = 0;
function watchFile() {
  fs.watchFile(f, (curr, prev) => {
    // Make sure there is at least one watch event that shows a changed mtime.
    if (curr.mtime <= prev.mtime) {
      return;
    }
    changes++;
    fs.unwatchFile(f);
    watchFile();
    fs.unwatchFile(f);
  });
}

watchFile();

function changeFile() {
  const fd = fs.openSync(f, 'w+');
  fs.writeSync(fd, 'xyz\n');
  fs.closeSync(fd);
}

changeFile();
const interval = setInterval(changeFile, 1000);
// Use unref() here so fs.watchFile() watcher is the only thing keeping the
// event loop open.
interval.unref();

process.on('exit', function() {
  assert.ok(changes > 0);
});
