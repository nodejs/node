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
// Can't test this when 'make test' doesn't assign a tty to the stdout.
// Yet another use-case for require('tty').spawn ?
const common = require('../common');
const assert = require('assert');
const readline = require('readline');

var key = {
  xterm: {
    home: ['\x1b[H', {ctrl: true, name: 'a'}],
    end: ['\x1b[F', {ctrl: true, name: 'e'}],
    metab: ['\x1bb', {meta: true, name: 'b'}],
    metaf: ['\x1bf', {meta: true, name: 'f'}],
    metad: ['\x1bd', {meta: true, name: 'd'}]
  },
  gnome: {
    home: ['\x1bOH', {ctrl: true, name: 'a'}],
    end: ['\x1bOF', {ctrl: true, name: 'e'}]
  },
  rxvt: {
    home: ['\x1b[7', {ctrl: true, name: 'a'}],
    end: ['\x1b[8', {ctrl: true, name: 'e'}]
  },
  putty: {
    home: ['\x1b[1~', {ctrl: true, name: 'a'}],
    end: ['\x1b[>~', {ctrl: true, name: 'e'}]
  }
};

var readlineFakeStream = function() {
  var written_bytes = [];
  var rl = readline.createInterface(
      { input: process.stdin,
        output: process.stdout,
        completer: function(text) {
          return [[], ''];
        }
      });
  var _stdoutWrite = process.stdout.write;
  process.stdout.write = function(data) {
    data.split('').forEach(rl.written_bytes.push.bind(rl.written_bytes));
    _stdoutWrite.apply(this, arguments);
  };
  rl.written_bytes = written_bytes;
  return rl;
};

var rl = readlineFakeStream();
var written_bytes_length, refreshed;

rl.write('foo');
assert.strictEqual(3, rl.cursor);
[key.xterm, key.rxvt, key.gnome, key.putty].forEach(function(key) {
  rl.write.apply(rl, key.home);
  assert.strictEqual(0, rl.cursor);
  rl.write.apply(rl, key.end);
  assert.strictEqual(3, rl.cursor);
});

rl = readlineFakeStream();
rl.write('foo bar.hop/zoo');
rl.write.apply(rl, key.xterm.home);
[
  {cursor: 4, key: key.xterm.metaf},
  {cursor: 7, key: key.xterm.metaf},
  {cursor: 8, key: key.xterm.metaf},
  {cursor: 11, key: key.xterm.metaf},
  {cursor: 12, key: key.xterm.metaf},
  {cursor: 15, key: key.xterm.metaf},
  {cursor: 12, key: key.xterm.metab},
  {cursor: 11, key: key.xterm.metab},
  {cursor: 8, key: key.xterm.metab},
  {cursor: 7, key: key.xterm.metab},
  {cursor: 4, key: key.xterm.metab},
  {cursor: 0, key: key.xterm.metab},
].forEach(function(action) {
  written_bytes_length = rl.written_bytes.length;
  rl.write.apply(rl, action.key);
  assert.strictEqual(action.cursor, rl.cursor);
  refreshed = written_bytes_length !== rl.written_bytes.length;
  assert.strictEqual(true, refreshed);
});

rl = readlineFakeStream();
rl.write('foo bar.hop/zoo');
rl.write.apply(rl, key.xterm.home);
[
  'bar.hop/zoo',
  '.hop/zoo',
  'hop/zoo',
  '/zoo',
  'zoo',
  ''
].forEach(function(expectedLine) {
  rl.write.apply(rl, key.xterm.metad);
  assert.strictEqual(0, rl.cursor);
  assert.strictEqual(expectedLine, rl.line);
});
rl.close();
