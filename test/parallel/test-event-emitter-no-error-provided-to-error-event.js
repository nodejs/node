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
const events = require('events');
const domain = require('domain');

{
  const e = new events.EventEmitter();
  const d = domain.create();
  d.add(e);
  d.on('error', common.mustCall(function(er) {
    assert(er instanceof Error, 'error created');
  }));
  e.emit('error');
}

for (const arg of [false, null, undefined]) {
  const e = new events.EventEmitter();
  const d = domain.create();
  d.add(e);
  d.on('error', common.mustCall(function(er) {
    assert(er instanceof Error, 'error created');
  }));
  e.emit('error', arg);
}

for (const arg of [42, 'fortytwo', true]) {
  const e = new events.EventEmitter();
  const d = domain.create();
  d.add(e);
  d.on('error', common.mustCall(function(er) {
    assert.strictEqual(er, arg);
  }));
  e.emit('error', arg);
}
