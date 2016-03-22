/**
 * https://github.com/nodejs/node/issues/5574
 *
 * Test that characters from stdin are not randomly lost when using a
 * `readline.Interface` and a `tty.ReadStream` on `process.stdin`
 */

'use strict';

var assert     = require('assert');
var Interface  = require('readline').Interface;
var Readable   = require('stream').Readable;
var ReadStream = require('tty').ReadStream;
var spawn      = require('child_process').spawn;

var spawnCat = require('../common').spawnCat;


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

    if (data !== null) return setTimeout(type, 60 * 1000 / kpm);
  }

  type();

  child.on('close', function(code, signal) {
    assert.equal(code, 0);
    assert.equal(signal, null);
    // cat on windows adds a \r\n at the end.
    assert.equal(output.trim(), input.trim());
  });
}

function parent() {
  Interface(process.stdin, process.stdout);

  var stdin = ReadStream(process.stdin.fd);
  process.stdin.pause();

  var stdio = [stdin, process.stdout];

  spawnCat({ stdio: stdio });
}
