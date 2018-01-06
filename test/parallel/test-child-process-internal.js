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
const common = require('../common');
const assert = require('assert');

//messages
const PREFIX = 'NODE_';
const normal = { cmd: `foo${PREFIX}` };
const internal = { cmd: `${PREFIX}bar` };

if (process.argv[2] === 'child') {
  //send non-internal message containing PREFIX at a non prefix position
  process.send(normal);

  //send internal message
  process.send(internal);

  process.exit(0);

} else {

  const fork = require('child_process').fork;
  const child = fork(process.argv[1], ['child']);

  child.once('message', common.mustCall(function(data) {
    assert.deepStrictEqual(data, normal);
  }));

  child.once('internalMessage', common.mustCall(function(data) {
    assert.deepStrictEqual(data, internal);
  }));
}
