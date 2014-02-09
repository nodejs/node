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

switch (process.argv[2]) {
  case 'child1':
    return child1();
  case 'child2':
    return child2();
  case 'child3':
    return child3();
  case 'child4':
    return child4();
  case 'child5':
    return child5();
  case undefined:
    return parent();
  default:
    throw new Error('wtf');
}

function child1() {
  process.exitCode = 42;
  process.on('exit', function(code) {
    assert.equal(code, 42);
  });
}

function child2() {
  process.exitCode = 99;
  process.on('exit', function(code) {
    assert.equal(code, 42);
  });
  process.exit(42);
}

function child3() {
  process.exitCode = 99;
  process.on('exit', function(code) {
    assert.equal(code, 0);
  });
  process.exit(0);
}

function child4() {
  process.exitCode = 99;
  process.on('exit', function(code) {
    if (code !== 1) {
      console.log('wrong code! expected 1 for uncaughtException');
      process.exit(99);
    }
  });
  throw new Error('ok');
}

function child5() {
  process.exitCode = 95;
  process.on('exit', function(code) {
    assert.equal(code, 95);
    process.exitCode = 99;
  });
}

function parent() {
  test('child1', 42);
  test('child2', 42);
  test('child3', 0);
  test('child4', 1);
  test('child5', 99);
}

function test(arg, exit) {
  var spawn = require('child_process').spawn;
  var node = process.execPath;
  var f = __filename;
  var option = { stdio: [ 0, 1, 'ignore' ] };
  spawn(node, [f, arg], option).on('exit', function(code) {
    assert.equal(code, exit, 'wrong exit for ' +
                 arg + '\nexpected:' + exit +
                 ' but got:' + code);
    console.log('ok - %s exited with %d', arg, exit);
  });
}
