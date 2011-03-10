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

var assert = require('assert'),
    util = require('util'),
    spawn = require('child_process').spawn;

// We're trying to reproduce:
// $ echo "hello\nnode\nand\nworld" | grep o | sed s/o/a/

var echo = spawn('echo', ['hello\nnode\nand\nworld\n']),
    grep = spawn('grep', ['o']),
    sed = spawn('sed', ['s/o/O/']);

/*
 * grep and sed hang if the spawn function leaks file descriptors to child
 * processes.
 * This happens when calling pipe(2) and then forgetting to set the
 * FD_CLOEXEC flag on the resulting file descriptors.
 *
 * This test checks child processes exit, meaning they don't hang like
 * explained above.
 */



// pipe echo | grep
echo.stdout.on('data', function(data) {
  if (!grep.stdin.write(data)) {
    echo.stdout.pause();
  }
});

grep.stdin.on('drain', function(data) {
  echo.stdout.resume();
});

// propagate end from echo to grep
echo.stdout.on('end', function(code) {
  grep.stdin.end();
});



// pipe grep | sed
grep.stdout.on('data', function(data) {
  if (!sed.stdin.write(data)) {
    grep.stdout.pause();
  }
});

sed.stdin.on('drain', function(data) {
  grep.stdout.resume();
});

// propagate end from grep to sed
grep.stdout.on('end', function(code) {
  sed.stdin.end();
});



var result = '';

// print sed's output
sed.stdout.on('data', function(data) {
  result += data.toString('utf8', 0, data.length);
  util.print(data);
});

sed.stdout.on('end', function(code) {
  assert.equal(result, 'hellO\nnOde\nwOrld\n');
});
