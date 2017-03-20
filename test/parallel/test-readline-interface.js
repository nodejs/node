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
const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const internalReadline = require('internal/readline');
const EventEmitter = require('events').EventEmitter;
const inherits = require('util').inherits;
const Writable = require('stream').Writable;
const Readable = require('stream').Readable;

function FakeInput() {
  EventEmitter.call(this);
}
inherits(FakeInput, EventEmitter);
FakeInput.prototype.resume = function() {};
FakeInput.prototype.pause = function() {};
FakeInput.prototype.write = function() {};
FakeInput.prototype.end = function() {};

function isWarned(emitter) {
  for (const name in emitter) {
    const listeners = emitter[name];
    if (listeners.warned) return true;
  }
  return false;
}

{
  // Default crlfDelay is 100ms
  const fi = new FakeInput();
  const rli = new readline.Interface({ input: fi, output: fi });
  assert.strictEqual(rli.crlfDelay, 100);
  rli.close();
}

{
  // Minimum crlfDelay is 100ms
  const fi = new FakeInput();
  const rli = new readline.Interface({ input: fi, output: fi, crlfDelay: 0});
  assert.strictEqual(rli.crlfDelay, 100);
  rli.close();
}

{
  // Maximum crlfDelay is 2000ms
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    crlfDelay: 1 << 30
  });
  assert.strictEqual(rli.crlfDelay, 2000);
  rli.close();
}

[ true, false ].forEach(function(terminal) {
  let fi;
  let rli;
  let called;

  // disable history
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal,
                                 historySize: 0 });
  assert.strictEqual(rli.historySize, 0);

  fi.emit('data', 'asdf\n');
  assert.deepStrictEqual(rli.history, terminal ? [] : undefined);
  rli.close();

  // default history size 30
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal});
  assert.strictEqual(rli.historySize, 30);

  fi.emit('data', 'asdf\n');
  assert.deepStrictEqual(rli.history, terminal ? ['asdf'] : undefined);
  rli.close();

  // sending a full line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.strictEqual(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);

  // sending a blank line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.strictEqual(line, '');
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
    assert.strictEqual(line, 'a');
  });
  fi.emit('data', 'a');
  assert.ok(!called);
  fi.emit('data', '\n');
  assert.ok(called);
  rli.close();

  // sending multiple newlines at once
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  let expectedLines = ['foo', 'bar', 'baz'];
  let callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n') + '\n');
  assert.strictEqual(callCount, expectedLines.length);
  rli.close();

  // sending multiple newlines at once that does not end with a new line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n'));
  assert.strictEqual(callCount, expectedLines.length - 1);
  rli.close();

  // sending multiple newlines at once that does not end with a new(empty)
  // line and a `end` event
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', ''];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  rli.on('close', function() {
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n'));
  fi.emit('end');
  assert.strictEqual(callCount, expectedLines.length);
  rli.close();

  // sending multiple newlines at once that does not end with a new line
  // and a `end` event(last line is)

  // \r\n should emit one line event, not two
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\r\n'));
  assert.strictEqual(callCount, expectedLines.length - 1);
  rli.close();

  // \r\n should emit one line event when split across multiple writes.
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  expectedLines.forEach(function(line) {
    fi.emit('data', line + '\r');
    fi.emit('data', '\n');
  });
  assert.strictEqual(callCount, expectedLines.length);
  rli.close();

  // \r should behave like \n when alone
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  expectedLines = ['foo', 'bar', 'baz', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\r'));
  assert.strictEqual(callCount, expectedLines.length - 1);
  rli.close();

  // \r at start of input should output blank line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  expectedLines = ['', 'foo' ];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', '\rfoo\r');
  assert.strictEqual(callCount, expectedLines.length);
  rli.close();

  // Emit two line events when the delay
  //   between \r and \n exceeds crlfDelay
  {
    const fi = new FakeInput();
    const delay = 200;
    const rli = new readline.Interface({
      input: fi,
      output: fi,
      terminal: terminal,
      crlfDelay: delay
    });
    let callCount = 0;
    rli.on('line', function(line) {
      callCount++;
    });
    fi.emit('data', '\r');
    setTimeout(common.mustCall(() => {
      fi.emit('data', '\n');
      assert.strictEqual(callCount, 2);
      rli.close();
    }), delay * 2);
  }

  // \t when there is no completer function should behave like an ordinary
  //   character
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  called = false;
  rli.on('line', function(line) {
    assert.strictEqual(line, '\t');
    assert.strictEqual(called, false);
    called = true;
  });
  fi.emit('data', '\t');
  fi.emit('data', '\n');
  assert.ok(called);
  rli.close();

  // \t does not become part of the input when there is a completer function
  fi = new FakeInput();
  const completer = function(line) {
    return [[], line];
  };
  rli = new readline.Interface({
    input: fi,
    output: fi,
    terminal: true,
    completer: completer
  });
  called = false;
  rli.on('line', function(line) {
    assert.strictEqual(line, 'foo');
    assert.strictEqual(called, false);
    called = true;
  });
  for (const character of '\tfo\to\t') {
    fi.emit('data', character);
  }
  fi.emit('data', '\n');
  assert.ok(called);
  rli.close();

  // constructor throws if completer is not a function or undefined
  fi = new FakeInput();
  assert.throws(function() {
    readline.createInterface({
      input: fi,
      completer: 'string is not valid'
    });
  }, function(err) {
    if (err instanceof TypeError) {
      if (/Argument "completer" must be a function/.test(err)) {
        return true;
      }
    }
    return false;
  });

  // duplicate lines are removed from history when
  // `options.removeHistoryDuplicates` is `true`
  fi = new FakeInput();
  rli = new readline.Interface({
    input: fi,
    output: fi,
    terminal: true,
    removeHistoryDuplicates: true
  });
  expectedLines = ['foo', 'bar', 'baz', 'bar', 'bat', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n') + '\n');
  assert.strictEqual(callCount, expectedLines.length);
  fi.emit('keypress', '.', { name: 'up' }); // 'bat'
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'bar'
  assert.notStrictEqual(rli.line, expectedLines[--callCount]);
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'baz'
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'foo'
  assert.notStrictEqual(rli.line, expectedLines[--callCount]);
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  assert.strictEqual(callCount, 0);
  rli.close();

  // duplicate lines are not removed from history when
  // `options.removeHistoryDuplicates` is `false`
  fi = new FakeInput();
  rli = new readline.Interface({
    input: fi,
    output: fi,
    terminal: true,
    removeHistoryDuplicates: false
  });
  expectedLines = ['foo', 'bar', 'baz', 'bar', 'bat', 'bat'];
  callCount = 0;
  rli.on('line', function(line) {
    assert.strictEqual(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n') + '\n');
  assert.strictEqual(callCount, expectedLines.length);
  fi.emit('keypress', '.', { name: 'up' }); // 'bat'
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'bar'
  assert.notStrictEqual(rli.line, expectedLines[--callCount]);
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'baz'
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'bar'
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  fi.emit('keypress', '.', { name: 'up' }); // 'foo'
  assert.strictEqual(rli.line, expectedLines[--callCount]);
  assert.strictEqual(callCount, 0);
  rli.close();

  // sending a multi-byte utf8 char over multiple writes
  const buf = Buffer.from('☮', 'utf8');
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  callCount = 0;
  rli.on('line', function(line) {
    callCount++;
    assert.strictEqual(line, buf.toString('utf8'));
  });
  [].forEach.call(buf, function(i) {
    fi.emit('data', Buffer.from([i]));
  });
  assert.strictEqual(callCount, 0);
  fi.emit('data', '\n');
  assert.strictEqual(callCount, 1);
  rli.close();

  // Regression test for repl freeze, #1968:
  // check that nothing fails if 'keypress' event throws.
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  const keys = [];
  fi.on('keypress', function(key) {
    keys.push(key);
    if (key === 'X') {
      throw new Error('bad thing happened');
    }
  });
  try {
    fi.emit('data', 'fooX');
  } catch (e) { }
  fi.emit('data', 'bar');
  assert.strictEqual(keys.join(''), 'fooXbar');
  rli.close();

  // calling readline without `new`
  fi = new FakeInput();
  rli = readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.strictEqual(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);
  rli.close();

  if (terminal) {
    // question
    fi = new FakeInput();
    rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
    expectedLines = ['foo'];
    rli.question(expectedLines[0], function() {
      rli.close();
    });
    let cursorPos = rli._getCursorPos();
    assert.strictEqual(cursorPos.rows, 0);
    assert.strictEqual(cursorPos.cols, expectedLines[0].length);
    rli.close();

    // sending a multi-line question
    fi = new FakeInput();
    rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
    expectedLines = ['foo', 'bar'];
    rli.question(expectedLines.join('\n'), function() {
      rli.close();
    });
    cursorPos = rli._getCursorPos();
    assert.strictEqual(cursorPos.rows, expectedLines.length - 1);
    assert.strictEqual(cursorPos.cols, expectedLines.slice(-1)[0].length);
    rli.close();
  }

  // isFullWidthCodePoint() should return false for non-numeric values
  [true, false, null, undefined, {}, [], 'あ'].forEach((v) => {
    assert.strictEqual(internalReadline.isFullWidthCodePoint('あ'), false);
  });

  // wide characters should be treated as two columns.
  assert.strictEqual(internalReadline.isFullWidthCodePoint('a'.charCodeAt(0)),
                     false);
  assert.strictEqual(internalReadline.isFullWidthCodePoint('あ'.charCodeAt(0)),
                     true);
  assert.strictEqual(internalReadline.isFullWidthCodePoint('谢'.charCodeAt(0)),
                     true);
  assert.strictEqual(internalReadline.isFullWidthCodePoint('고'.charCodeAt(0)),
                     true);
  assert.strictEqual(internalReadline.isFullWidthCodePoint(0x1f251), true);
  assert.strictEqual(internalReadline.getStringWidth('abcde'), 5);
  assert.strictEqual(internalReadline.getStringWidth('古池や'), 6);
  assert.strictEqual(internalReadline.getStringWidth('ノード.js'), 9);
  assert.strictEqual(internalReadline.getStringWidth('你好'), 4);
  assert.strictEqual(internalReadline.getStringWidth('안녕하세요'), 10);
  assert.strictEqual(internalReadline.getStringWidth('A\ud83c\ude00BC'), 5);

  // check if vt control chars are stripped
  assert.strictEqual(
    internalReadline.stripVTControlCharacters('\u001b[31m> \u001b[39m'),
    '> '
  );
  assert.strictEqual(
    internalReadline.stripVTControlCharacters('\u001b[31m> \u001b[39m> '),
    '> > '
  );
  assert.strictEqual(
    internalReadline.stripVTControlCharacters('\u001b[31m\u001b[39m'),
    ''
  );
  assert.strictEqual(
    internalReadline.stripVTControlCharacters('> '),
    '> '
  );
  assert.strictEqual(internalReadline
    .getStringWidth('\u001b[31m> \u001b[39m'), 2);
  assert.strictEqual(internalReadline
    .getStringWidth('\u001b[31m> \u001b[39m> '), 4);
  assert.strictEqual(internalReadline
    .getStringWidth('\u001b[31m\u001b[39m'), 0);
  assert.strictEqual(internalReadline.getStringWidth('> '), 2);

  assert.deepStrictEqual(fi.listeners(terminal ? 'keypress' : 'data'), []);

  // check EventEmitter memory leak
  for (let i = 0; i < 12; i++) {
    const rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });
    rl.close();
    assert.strictEqual(isWarned(process.stdin._events), false);
    assert.strictEqual(isWarned(process.stdout._events), false);
  }

  //can create a new readline Interface with a null output arugument
  fi = new FakeInput();
  rli = new readline.Interface({input: fi, output: null, terminal: terminal });

  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.strictEqual(line, 'asdf');
  });
  fi.emit('data', 'asdf\n');
  assert.ok(called);

  assert.doesNotThrow(function() {
    rli.setPrompt('ddd> ');
  });

  assert.doesNotThrow(function() {
    rli.prompt();
  });

  assert.doesNotThrow(function() {
    rli.write('really shouldnt be seeing this');
  });

  assert.doesNotThrow(function() {
    rli.question('What do you think of node.js? ', function(answer) {
      console.log('Thank you for your valuable feedback:', answer);
      rli.close();
    });
  });

  {
    const expected = terminal ?
      ['\u001b[1G', '\u001b[0J', '$ ', '\u001b[3G'] :
      ['$ '];

    let counter = 0;
    const output = new Writable({
      write: common.mustCall((chunk, enc, cb) => {
        assert.strictEqual(chunk.toString(), expected[counter++]);
        cb();
        rl.close();
      }, expected.length)
    });

    const rl = readline.createInterface({
      input: new Readable({ read: () => {} }),
      output: output,
      prompt: '$ ',
      terminal: terminal
    });

    rl.prompt();

    assert.strictEqual(rl._prompt, '$ ');
  }
});
