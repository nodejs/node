'use strict';
// Can't test this when 'make test' doesn't assign a tty to the stdout.
// Yet another use-case for require('tty').spawn ?
var common = require('../common');
var assert = require('assert');
var readline = require('readline');

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
assert.equal(3, rl.cursor);
[key.xterm, key.rxvt, key.gnome, key.putty].forEach(function(key) {
  rl.write.apply(rl, key.home);
  assert.equal(0, rl.cursor);
  rl.write.apply(rl, key.end);
  assert.equal(3, rl.cursor);
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
  assert.equal(action.cursor, rl.cursor);
  refreshed = written_bytes_length !== rl.written_bytes.length;
  assert.equal(true, refreshed);
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
  assert.equal(0, rl.cursor);
  assert.equal(expectedLine, rl.line);
});
rl.close();
