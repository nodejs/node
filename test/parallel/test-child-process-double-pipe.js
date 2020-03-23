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
const {
  isWindows,
  mustCall,
  mustCallAtLeast,
} = require('../common');
const assert = require('assert');
const os = require('os');
const spawn = require('child_process').spawn;
const debug = require('util').debuglog('test');

// We're trying to reproduce:
// $ echo "hello\nnode\nand\nworld" | grep o | sed s/o/a/

let grep, sed, echo;

if (isWindows) {
  grep = spawn('grep', ['--binary', 'o']);
  sed = spawn('sed', ['--binary', 's/o/O/']);
  echo = spawn('cmd.exe',
               ['/c', 'echo', 'hello&&', 'echo',
                'node&&', 'echo', 'and&&', 'echo', 'world']);
} else {
  grep = spawn('grep', ['o']);
  sed = spawn('sed', ['s/o/O/']);
  echo = spawn('echo', ['hello\nnode\nand\nworld\n']);
}

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
echo.stdout.on('data', mustCallAtLeast((data) => {
  debug(`grep stdin write ${data.length}`);
  if (!grep.stdin.write(data)) {
    echo.stdout.pause();
  }
}));

// TODO(@jasnell): This does not appear to ever be
// emitted. It's not clear if it is necessary.
grep.stdin.on('drain', (data) => {
  echo.stdout.resume();
});

// Propagate end from echo to grep
echo.stdout.on('end', mustCall((code) => {
  grep.stdin.end();
}));

echo.on('exit', mustCall(() => {
  debug('echo exit');
}));

grep.on('exit', mustCall(() => {
  debug('grep exit');
}));

sed.on('exit', mustCall(() => {
  debug('sed exit');
}));


// pipe grep | sed
grep.stdout.on('data', mustCallAtLeast((data) => {
  debug(`grep stdout ${data.length}`);
  if (!sed.stdin.write(data)) {
    grep.stdout.pause();
  }
}));

// TODO(@jasnell): This does not appear to ever be
// emitted. It's not clear if it is necessary.
sed.stdin.on('drain', (data) => {
  grep.stdout.resume();
});

// Propagate end from grep to sed
grep.stdout.on('end', mustCall((code) => {
  debug('grep stdout end');
  sed.stdin.end();
}));


let result = '';

// print sed's output
sed.stdout.on('data', mustCallAtLeast((data) => {
  result += data.toString('utf8', 0, data.length);
  debug(data);
}));

sed.stdout.on('end', mustCall((code) => {
  assert.strictEqual(result, `hellO${os.EOL}nOde${os.EOL}wOrld${os.EOL}`);
}));
