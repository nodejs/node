'use strict';
const common = require('../common');
const assert = require('assert');
const os = require('os');
const util = require('util');
const spawn = require('child_process').spawn;

// We're trying to reproduce:
// $ echo "hello\nnode\nand\nworld" | grep o | sed s/o/a/

var grep, sed, echo;

if (common.isWindows) {
  grep = spawn('grep', ['--binary', 'o']),
  sed = spawn('sed', ['--binary', 's/o/O/']),
  echo = spawn('cmd.exe',
               ['/c', 'echo', 'hello&&', 'echo',
                'node&&', 'echo', 'and&&', 'echo', 'world']);
} else {
  grep = spawn('grep', ['o']),
  sed = spawn('sed', ['s/o/O/']),
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
echo.stdout.on('data', function(data) {
  console.error('grep stdin write ' + data.length);
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

echo.on('exit', function() {
  console.error('echo exit');
});

grep.on('exit', function() {
  console.error('grep exit');
});

sed.on('exit', function() {
  console.error('sed exit');
});


// pipe grep | sed
grep.stdout.on('data', function(data) {
  console.error('grep stdout ' + data.length);
  if (!sed.stdin.write(data)) {
    grep.stdout.pause();
  }
});

sed.stdin.on('drain', function(data) {
  grep.stdout.resume();
});

// propagate end from grep to sed
grep.stdout.on('end', function(code) {
  console.error('grep stdout end');
  sed.stdin.end();
});


var result = '';

// print sed's output
sed.stdout.on('data', function(data) {
  result += data.toString('utf8', 0, data.length);
  util.print(data);
});

sed.stdout.on('end', function(code) {
  assert.equal(result, 'hellO' + os.EOL + 'nOde' + os.EOL + 'wOrld' + os.EOL);
});
