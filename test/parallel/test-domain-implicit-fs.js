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
// Simple tests of most basic domain functionality.

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

process.on('warning', common.mustNotCall());

const d = new domain.Domain();

d.on('error', common.mustCall(function(er) {
  console.error('caught', er);

  assert.strictEqual(er.domain, d);
  assert.strictEqual(er.domainThrown, true);
  assert.ok(!er.domainEmitter);
  assert.strictEqual(er.actual.code, 'ENOENT');
  assert.match(er.actual.path, /\bthis file does not exist\b/i);
  assert.strictEqual(typeof er.actual.errno, 'number');
}));


// Implicit handling of thrown errors while in a domain, via the
// single entry points of ReqWrap and MakeCallback.  Even if
// we try very hard to escape, there should be no way to, even if
// we go many levels deep through timeouts and multiple IO calls.
// Everything that happens between the domain.enter() and domain.exit()
// calls will be bound to the domain, even if multiple levels of
// handles are created.
d.run(function() {
  setTimeout(function() {
    const fs = require('fs');
    fs.readdir(__dirname, function() {
      fs.open('this file does not exist', 'r', function(er) {
        assert.ifError(er);
        throw new Error('should not get here!');
      });
    });
  }, 100);
});
