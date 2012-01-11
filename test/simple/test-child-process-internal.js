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

//messages
var PREFIX = 'NODE_';
var normal = {cmd: 'foo' + PREFIX};
var internal = {cmd: PREFIX + 'bar'};

if (process.argv[2] === 'child') {
  //send non-internal message containing PREFIX at a non prefix position
  process.send(normal);

  //send inernal message
  process.send(internal);

  process.exit(0);

} else {

  var fork = require('child_process').fork;
  var child = fork(process.argv[1], ['child']);

  var gotNormal;
  child.once('message', function(data) {
    gotNormal = data;
  });

  var gotInternal;
  child.once('internalMessage', function(data) {
    gotInternal = data;
  });

  process.on('exit', function() {
    assert.deepEqual(gotNormal, normal);
    assert.deepEqual(gotInternal, internal);
  });
}
