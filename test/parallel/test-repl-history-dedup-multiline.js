'use strict';

const common = require('../common');

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

const assert = require('assert');
const readline = require('readline');
const { EventEmitter } = require('events');

class FakeInput extends EventEmitter {
  resume() {}
  pause() {}
  write() {}
  end() {}
}
FakeInput.prototype.readable = true;

{
  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    terminal: true,
    removeHistoryDuplicates: true,
  });

  function submitLine(line) {
    rli.line = line;
    fi.emit('keypress', '', { name: 'enter' });
  }

  submitLine('line1\nline2');
  submitLine('other');
  submitLine('line1\nline2');

  assert.strictEqual(rli.history.length, 2);
  assert.strictEqual(rli.history[0], 'line2\rline1');
  assert.strictEqual(rli.history[1], 'other');

  rli.close();
}
