'use strict';
const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const { PassThrough } = require('stream');
const promises = readline.promises;

{
  // Verify the shape of the promises API.
  assert.strictEqual(promises.clearLine, readline.clearLine);
  assert.strictEqual(promises.clearScreenDown, readline.clearScreenDown);
  assert.strictEqual(promises.cursorTo, readline.cursorTo);
  assert.strictEqual(promises.emitKeypressEvents, readline.emitKeypressEvents);
  assert.strictEqual(promises.moveCursor, readline.moveCursor);
  assert.strictEqual(typeof promises.createInterface, 'function');
  assert.notStrictEqual(promises.createInterface, readline.createInterface);
  assert.strictEqual(typeof promises.Interface, 'function');
  assert.notStrictEqual(promises.Interface, readline.Interface);

  assert.strictEqual(promises.Interface.prototype.close,
                     readline.Interface.prototype.close);
  assert.strictEqual(promises.Interface.prototype.pause,
                     readline.Interface.prototype.pause);
  assert.strictEqual(promises.Interface.prototype.prompt,
                     readline.Interface.prototype.prompt);
  assert.strictEqual(promises.Interface.prototype.resume,
                     readline.Interface.prototype.resume);
  assert.strictEqual(promises.Interface.prototype.setPrompt,
                     readline.Interface.prototype.setPrompt);
  assert.strictEqual(promises.Interface.prototype.write,
                     readline.Interface.prototype.write);
  assert.strictEqual(promises.Interface.prototype[Symbol.asyncIterator],
                     readline.Interface.prototype[Symbol.asyncIterator]);
  assert.strictEqual(typeof promises.Interface.prototype.question, 'function');
  assert.notStrictEqual(promises.Interface.prototype.question,
                        readline.Interface.prototype.question);
  assert.strictEqual(typeof promises.Interface.prototype._tabComplete,
                     'function');
  assert.notStrictEqual(promises.Interface.prototype._tabComplete,
                        readline.Interface.prototype._tabComplete);
}

{
  const input = new PassThrough();
  const rl = promises.createInterface({
    terminal: true,
    input: input
  });

  rl.on('line', common.mustCall((data) => {
    assert.strictEqual(data, 'abc');
  }));

  input.end('abc');
}

{
  const input = new PassThrough();
  const rl = promises.createInterface({
    terminal: true,
    input: input
  });

  rl.on('line', common.mustNotCall('must not be called before newline'));

  input.write('abc');
}

{
  const input = new PassThrough();
  const rl = promises.createInterface({
    terminal: true,
    input: input
  });

  rl.on('line', common.mustCall((data) => {
    assert.strictEqual(data, 'abc');
  }));

  input.write('abc\n');
}

{
  const input = new PassThrough();
  const rl = promises.createInterface({
    terminal: true,
    input: input
  });

  rl.write('foo');
  assert.strictEqual(rl.cursor, 3);

  const key = {
    xterm: {
      home: ['\x1b[H', { ctrl: true, name: 'a' }],
      end: ['\x1b[F', { ctrl: true, name: 'e' }],
    },
    gnome: {
      home: ['\x1bOH', { ctrl: true, name: 'a' }],
      end: ['\x1bOF', { ctrl: true, name: 'e' }]
    },
    rxvt: {
      home: ['\x1b[7', { ctrl: true, name: 'a' }],
      end: ['\x1b[8', { ctrl: true, name: 'e' }]
    },
    putty: {
      home: ['\x1b[1~', { ctrl: true, name: 'a' }],
      end: ['\x1b[>~', { ctrl: true, name: 'e' }]
    }
  };

  [key.xterm, key.gnome, key.rxvt, key.putty].forEach(function(key) {
    rl.write.apply(rl, key.home);
    assert.strictEqual(rl.cursor, 0);
    rl.write.apply(rl, key.end);
    assert.strictEqual(rl.cursor, 3);
  });

}

{
  const input = new PassThrough();
  const rl = promises.createInterface({
    terminal: true,
    input: input
  });

  const key = {
    xterm: {
      home: ['\x1b[H', { ctrl: true, name: 'a' }],
      metab: ['\x1bb', { meta: true, name: 'b' }],
      metaf: ['\x1bf', { meta: true, name: 'f' }],
    }
  };

  rl.write('foo bar.hop/zoo');
  rl.write.apply(rl, key.xterm.home);
  [
    { cursor: 4, key: key.xterm.metaf },
    { cursor: 7, key: key.xterm.metaf },
    { cursor: 8, key: key.xterm.metaf },
    { cursor: 11, key: key.xterm.metaf },
    { cursor: 12, key: key.xterm.metaf },
    { cursor: 15, key: key.xterm.metaf },
    { cursor: 12, key: key.xterm.metab },
    { cursor: 11, key: key.xterm.metab },
    { cursor: 8, key: key.xterm.metab },
    { cursor: 7, key: key.xterm.metab },
    { cursor: 4, key: key.xterm.metab },
    { cursor: 0, key: key.xterm.metab },
  ].forEach(function(action) {
    rl.write.apply(rl, action.key);
    assert.strictEqual(rl.cursor, action.cursor);
  });
}

{
  const input = new PassThrough();
  const rl = promises.createInterface({
    terminal: true,
    input: input
  });

  const key = {
    xterm: {
      home: ['\x1b[H', { ctrl: true, name: 'a' }],
      metad: ['\x1bd', { meta: true, name: 'd' }]
    }
  };

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
    assert.strictEqual(rl.cursor, 0);
    assert.strictEqual(rl.line, expectedLine);
  });
}
