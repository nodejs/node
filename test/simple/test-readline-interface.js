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
FakeInput.prototype.pause = function() {};
FakeInput.prototype.write = function() {};
FakeInput.prototype.end = function() {};

[ true, false ].forEach(function(terminal) {
  var fi;
  var rli;
  var called;

  // sending a full line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);

  // sending a blank line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, '');
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
  rli.close();

  // sending a single character with no newline and then a newline
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'a');
  });
  fi.emit('data', 'a');
  assert.ok(!called);
  fi.emit('data', '\n');
  assert.ok(called);
  rli.close();

  // sending multiple newlines at once
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  var expectedLines = ['foo', 'bar', 'baz'];
  var callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n') + '\n');
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // sending multiple newlines at once that does not end with a new line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n'));
  assert.equal(callCount, expectedLines.length - 1);
  rli.close();

  // \r\n should emit one line event, not two
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\r\n'));
  assert.equal(callCount, expectedLines.length - 1);
  rli.close();

  // \r\n should emit one line event when split across multiple writes.
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  expectedLines.forEach(function(line) {
    fi.emit('data', line + '\r');
    fi.emit('data', '\n');
  });
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // \r should behave like \n when alone
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\r'));
  assert.equal(callCount, expectedLines.length - 1);
  rli.close();


  // sending a multi-byte utf8 char over multiple writes
  var buf = Buffer('☮', 'utf8');
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  callCount = 0;
  rli.on('line', function(line) {
    callCount++;
    assert.equal(line, buf.toString('utf8'));
  });
  [].forEach.call(buf, function(i) {
    fi.emit('data', Buffer([i]));
  });
  assert.equal(callCount, 0);
  fi.emit('data', '\n');
  assert.equal(callCount, 1);
  rli.close();

  assert.deepEqual(fi.listeners('end'), []);
  assert.deepEqual(fi.listeners(terminal ? 'keypress' : 'data'), []);
});
