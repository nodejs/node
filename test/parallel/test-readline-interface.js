'use strict';
const common = require('../common');
const assert = require('assert');
const readline = require('readline');
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
  for (var name in emitter) {
    var listeners = emitter[name];
    if (listeners.warned) return true;
  }
  return false;
}

[ true, false ].forEach(function(terminal) {
  var fi;
  var rli;
  var called;

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

  // sending multiple newlines at once that does not end with a new(empty)
  // line and a `end` event
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  expectedLines = ['foo', 'bar', 'baz', ''];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  rli.on('close', function() {
    callCount++;
  });
  fi.emit('data', expectedLines.join('\n'));
  fi.emit('end');
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // sending multiple newlines at once that does not end with a new line
  // and a `end` event(last line is)

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

  // \r at start of input should output blank line
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  expectedLines = ['', 'foo' ];
  callCount = 0;
  rli.on('line', function(line) {
    assert.equal(line, expectedLines[callCount]);
    callCount++;
  });
  fi.emit('data', '\rfoo\r');
  assert.equal(callCount, expectedLines.length);
  rli.close();

  // \t when there is no completer function should behave like an ordinary
  //   character
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  called = false;
  rli.on('line', function(line) {
    assert.equal(line, '\t');
    assert.strictEqual(called, false);
    called = true;
  });
  fi.emit('data', '\t');
  fi.emit('data', '\n');
  assert.ok(called);
  rli.close();

  // \t does not become part of the input when there is a completer function
  fi = new FakeInput();
  var completer = function(line) {
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
    assert.equal(line, 'foo');
    assert.strictEqual(called, false);
    called = true;
  });
  for (var character of '\tfo\to\t') {
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

  // sending a multi-byte utf8 char over multiple writes
  var buf = Buffer.from('☮', 'utf8');
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
  callCount = 0;
  rli.on('line', function(line) {
    callCount++;
    assert.equal(line, buf.toString('utf8'));
  });
  [].forEach.call(buf, function(i) {
    fi.emit('data', Buffer.from([i]));
  });
  assert.equal(callCount, 0);
  fi.emit('data', '\n');
  assert.equal(callCount, 1);
  rli.close();

  // Regression test for repl freeze, #1968:
  // check that nothing fails if 'keypress' event throws.
  fi = new FakeInput();
  rli = new readline.Interface({ input: fi, output: fi, terminal: true });
  var keys = [];
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
  assert.equal(keys.join(''), 'fooXbar');
  rli.close();

  // calling readline without `new`
  fi = new FakeInput();
  rli = readline.Interface({ input: fi, output: fi, terminal: terminal });
  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'asdf');
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
    var cursorPos = rli._getCursorPos();
    assert.equal(cursorPos.rows, 0);
    assert.equal(cursorPos.cols, expectedLines[0].length);
    rli.close();

    // sending a multi-line question
    fi = new FakeInput();
    rli = new readline.Interface({ input: fi, output: fi, terminal: terminal });
    expectedLines = ['foo', 'bar'];
    rli.question(expectedLines.join('\n'), function() {
      rli.close();
    });
    cursorPos = rli._getCursorPos();
    assert.equal(cursorPos.rows, expectedLines.length - 1);
    assert.equal(cursorPos.cols, expectedLines.slice(-1)[0].length);
    rli.close();
  }

  // isFullWidthCodePoint() should return false for non-numeric values
  [true, false, null, undefined, {}, [], 'あ'].forEach((v) => {
    assert.strictEqual(readline.isFullWidthCodePoint('あ'), false);
  });

  // wide characters should be treated as two columns.
  assert.equal(readline.isFullWidthCodePoint('a'.charCodeAt(0)), false);
  assert.equal(readline.isFullWidthCodePoint('あ'.charCodeAt(0)), true);
  assert.equal(readline.isFullWidthCodePoint('谢'.charCodeAt(0)), true);
  assert.equal(readline.isFullWidthCodePoint('고'.charCodeAt(0)), true);
  assert.equal(readline.isFullWidthCodePoint(0x1f251), true); // surrogate
  assert.equal(readline.codePointAt('ABC', 0), 0x41);
  assert.equal(readline.codePointAt('あいう', 1), 0x3044);
  assert.equal(readline.codePointAt('\ud800\udc00', 0),  // surrogate
      0x10000);
  assert.equal(readline.codePointAt('\ud800\udc00A', 2), // surrogate
      0x41);
  assert.equal(readline.getStringWidth('abcde'), 5);
  assert.equal(readline.getStringWidth('古池や'), 6);
  assert.equal(readline.getStringWidth('ノード.js'), 9);
  assert.equal(readline.getStringWidth('你好'), 4);
  assert.equal(readline.getStringWidth('안녕하세요'), 10);
  assert.equal(readline.getStringWidth('A\ud83c\ude00BC'), 5); // surrogate

  // check if vt control chars are stripped
  assert.strictEqual(
    readline.stripVTControlCharacters('\u001b[31m> \u001b[39m'),
    '> '
  );
  assert.strictEqual(
    readline.stripVTControlCharacters('\u001b[31m> \u001b[39m> '),
    '> > '
  );
  assert.strictEqual(
    readline.stripVTControlCharacters('\u001b[31m\u001b[39m'),
    ''
  );
  assert.strictEqual(
    readline.stripVTControlCharacters('> '),
    '> '
  );
  assert.equal(readline.getStringWidth('\u001b[31m> \u001b[39m'), 2);
  assert.equal(readline.getStringWidth('\u001b[31m> \u001b[39m> '), 4);
  assert.equal(readline.getStringWidth('\u001b[31m\u001b[39m'), 0);
  assert.equal(readline.getStringWidth('> '), 2);

  assert.deepStrictEqual(fi.listeners(terminal ? 'keypress' : 'data'), []);

  // check EventEmitter memory leak
  for (var i = 0; i < 12; i++) {
    var rl = readline.createInterface({
      input: process.stdin,
      output: process.stdout
    });
    rl.close();
    assert.equal(isWarned(process.stdin._events), false);
    assert.equal(isWarned(process.stdout._events), false);
  }

  //can create a new readline Interface with a null output arugument
  fi = new FakeInput();
  rli = new readline.Interface({input: fi, output: null, terminal: terminal });

  called = false;
  rli.on('line', function(line) {
    called = true;
    assert.equal(line, 'asdf');
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
    const expected = terminal
      ? ['\u001b[1G', '\u001b[0J', '$ ', '\u001b[3G']
      : ['$ '];

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
