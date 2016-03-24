'use strict';
// Refs: https://github.com/nodejs/node/issues/5574

/**
 * Test that characters from stdin are not randomly lost when using a
 * `readline.Interface` and a `tty.ReadStream` on `process.stdin`
 */

var Interface   = require('readline').Interface;
var Readable    = require('stream').Readable;
var ReadStream  = require('tty').ReadStream;
var spawn       = require('child_process').spawn;
var strictEqual = require('assert').strictEqual;

var common = require('../common');


const kpm = 60;
const input = `1234567890
qwertyuiop
asdfghjklñ
zxcvbnm`;


if (process.argv[2] === 'parent')
  parent();
else
  grandparent();

function grandparent() {
  var child = spawn(process.execPath, [__filename, 'parent']);
  child.stderr.pipe(process.stderr);

  var stdin = Readable({read: function() {}});
  stdin.pipe(child.stdin);

  var output = '';
  child.stdout.on('data', function(chunk) {
    output += chunk;
  });
  child.stdout.setEncoding('utf8');

  var index = 0;
  function type() {
    var data = input[index] || null;

    stdin.push(data);
    index++;

    if (data !== null)
      return setTimeout(type, 60 * 1000 / kpm);
  }

  type();

  child.on('close', common.mustCall(function(code, signal) {
    strictEqual(signal, null);
    strictEqual(code, 0);
    // cat on windows adds a \r\n at the end.
    strictEqual(output.trim(), input.trim());
  }));
}

function parent() {
  Interface(process.stdin, process.stdout);

  var stdin = ReadStream(process.stdin.fd);
  process.stdin.pause();

  var stdio = [stdin, process.stdout];

  common.spawnCat({ stdio: stdio });
}
