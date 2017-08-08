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

// Flags: --expose_internals
'use strict';
require('../common');

// These test cases are in `sequential` rather than the analogous test file in
// `parallel` because they become unrelaible under load. The unreliability under
// load was determined empirically when the test cases were in `parallel` by
// running:
//   tools/test.py -j 96 --repeat 192 test/parallel/test-readline-interface.js

const assert = require('assert');
const readline = require('readline');
const EventEmitter = require('events').EventEmitter;
const inherits = require('util').inherits;

function FakeInput() {
  EventEmitter.call(this);
}
inherits(FakeInput, EventEmitter);
FakeInput.prototype.resume = () => {};
FakeInput.prototype.pause = () => {};
FakeInput.prototype.write = () => {};
FakeInput.prototype.end = () => {};

[ true, false ].forEach(function(terminal) {
  // sending multiple newlines at once that does not end with a new line
  // and a `end` event(last line is)

  // \r\n should emit one line event, not two
  {
    const fi = new FakeInput();
    const rli = new readline.Interface(
      { input: fi, output: fi, terminal: terminal }
    );
    const expectedLines = ['foo', 'bar', 'baz', 'bat'];
    let callCount = 0;
    rli.on('line', function(line) {
      assert.strictEqual(line, expectedLines[callCount]);
      callCount++;
    });
    fi.emit('data', expectedLines.join('\r\n'));
    assert.strictEqual(callCount, expectedLines.length - 1);
    rli.close();
  }

  // \r\n should emit one line event when split across multiple writes.
  {
    const fi = new FakeInput();
    const rli = new readline.Interface(
      { input: fi, output: fi, terminal: terminal }
    );
    const expectedLines = ['foo', 'bar', 'baz', 'bat'];
    let callCount = 0;
    rli.on('line', function(line) {
      assert.strictEqual(line, expectedLines[callCount]);
      callCount++;
    });
    expectedLines.forEach(function(line) {
      fi.emit('data', `${line}\r`);
      fi.emit('data', '\n');
    });
    assert.strictEqual(callCount, expectedLines.length);
    rli.close();
  }
});
