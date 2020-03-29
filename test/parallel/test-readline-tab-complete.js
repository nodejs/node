'use strict';

// Flags: --expose-internals

const common = require('../common');
const readline = require('readline');
const assert = require('assert');
const EventEmitter = require('events').EventEmitter;
const { getStringWidth } = require('internal/util/inspect');

// This test verifies that the tab completion supports unicode and the writes
// are limited to the minimum.
[
  'あ',
  '𐐷',
  '🐕'
].forEach((char) => {
  [true, false].forEach((lineBreak) => {
    const completer = (line) => [
      [
        'First group',
        '',
        `${char}${'a'.repeat(10)}`, `${char}${'b'.repeat(10)}`, char.repeat(11),
      ],
      line
    ];

    let output = '';
    const width = getStringWidth(char) - 1;

    class FakeInput extends EventEmitter {
    columns = ((width + 1) * 10 + (lineBreak ? 0 : 10)) * 3

    write = common.mustCall((data) => {
      output += data;
    }, 6)

    resume() {}
    pause() {}
    end() {}
    }

    const fi = new FakeInput();
    const rli = new readline.Interface({
      input: fi,
      output: fi,
      terminal: true,
      completer: completer
    });

    const last = '\r\nFirst group\r\n\r\n' +
    `${char}${'a'.repeat(10)}${' '.repeat(2 + width * 10)}` +
      `${char}${'b'.repeat(10)}` +
      (lineBreak ? '\r\n' : ' '.repeat(2 + width * 10)) +
      `${char.repeat(11)}\r\n` +
    `\r\n\u001b[1G\u001b[0J> ${char}\u001b[${4 + width}G`;

    const expectations = [char, '', last];

    rli.on('line', common.mustNotCall());
    for (const character of `${char}\t\t`) {
      fi.emit('data', character);
      assert.strictEqual(output, expectations.shift());
      output = '';
    }
    rli.close();
  });
});
