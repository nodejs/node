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
var readline = require('readline');
var EventEmitter = require('events').EventEmitter;
var inherits = require('util').inherits;

function FakeInput() {
  EventEmitter.call(this);
}
inherits(FakeInput, EventEmitter);
FakeInput.prototype.resume = function() {};

var fi;
var rli;
var called;

// sending a full line
fi = new FakeInput();
rli = new readline.Interface(fi, {});
called = false;
rli.on('line', function(line) {
  called = true;
  assert.equal(line, 'asdf\n');
});
fi.emit('data', 'asdf\n');
assert.ok(called);

// sending a blank line
fi = new FakeInput();
rli = new readline.Interface(fi, {});
called = false;
rli.on('line', function(line) {
  called = true;
  assert.equal(line, '\n');
});
fi.emit('data', '\n');
assert.ok(called);

// sending a single character with no newline
fi = new FakeInput();
rli = new readline.Interface(fi, {});
called = false;
rli.on('line', function(line) {
  called = true;
});
fi.emit('data', 'a');
assert.ok(!called);

// sending a single character with no newline and then a newline
fi = new FakeInput();
rli = new readline.Interface(fi, {});
called = false;
rli.on('line', function(line) {
  called = true;
  assert.equal(line, 'a\n');
});
fi.emit('data', 'a');
assert.ok(!called);
fi.emit('data', '\n');
assert.ok(called);

// sending multiple newlines at once
fi = new FakeInput();
rli = new readline.Interface(fi, {});
var expectedLines = ['foo\n', 'bar\n', 'baz\n'];
var callCount = 0;
rli.on('line', function(line) {
  assert.equal(line, expectedLines[callCount]);
  callCount++;
});
fi.emit('data', expectedLines.join(''));
assert.equal(callCount, expectedLines.length);

// sending multiple newlines at once that does not end with a new line
fi = new FakeInput();
rli = new readline.Interface(fi, {});
var expectedLines = ['foo\n', 'bar\n', 'baz\n', 'bat'];
var callCount = 0;
rli.on('line', function(line) {
  assert.equal(line, expectedLines[callCount]);
  callCount++;
});
fi.emit('data', expectedLines.join(''));
assert.equal(callCount, expectedLines.length - 1);
