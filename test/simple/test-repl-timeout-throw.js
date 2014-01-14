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

var assert = require('assert');
var common = require('../common.js');

var spawn = require('child_process').spawn;

var child = spawn(process.execPath, [ '-i' ], {
  stdio: [null, null, 2]
});

var stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', function(c) {
  process.stdout.write(c);
  stdout += c;
});

child.stdin.write = function(original) { return function(c) {
  process.stderr.write(c);
  return original.call(child.stdin, c);
}}(child.stdin.write);

child.stdout.once('data', function() {
  child.stdin.write('var throws = 0;');
  child.stdin.write('process.on("exit",function(){console.log(throws)});');
  child.stdin.write('function thrower(){console.log("THROW",throws++);XXX};');
  child.stdin.write('setTimeout(thrower);""\n');

  setTimeout(fsTest, 50);
  function fsTest() {
    var f = JSON.stringify(__filename);
    child.stdin.write('fs.readFile(' + f + ', thrower);\n');
    setTimeout(eeTest, 50);
  }

  function eeTest() {
    child.stdin.write('setTimeout(function() {\n' +
                      '  var events = require("events");\n' +
                      '  var e = new events.EventEmitter;\n' +
                      '  process.nextTick(function() {\n' +
                      '    e.on("x", thrower);\n' +
                      '    setTimeout(function() {\n' +
                      '      e.emit("x");\n' +
                      '    });\n' +
                      '  });\n' +
                      '});"";\n');

    setTimeout(child.stdin.end.bind(child.stdin), 200);
  }
});

child.on('close', function(c) {
  assert(!c);
  // make sure we got 3 throws, in the end.
  var lastLine = stdout.trim().split(/\r?\n/).pop();
  assert.equal(lastLine, '> 3');
});
