'use strict';

// Flags: --expose-internals

const common = require('../common');
const readline = require('readline');
const assert = require('assert');
const EventEmitter = require('events').EventEmitter;
const { getStringWidth } = require('internal/util/inspect');

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

// This test verifies that the tab completion supports unicode and the writes
// are limited to the minimum.
[
  'あ',
  '𐐷',
  '🐕',
].forEach((char) => {
  [true, false].forEach((lineBreak) => {
    const completer = (line) => [
      [
        'First group',
        '',
        `${char}${'a'.repeat(10)}`, `${char}${'b'.repeat(10)}`, char.repeat(11),
      ],
      line,
    ];

    let output = '';
    const width = getStringWidth(char) - 1;

    class FakeInput extends EventEmitter {
      columns = ((width + 1) * 10 + (lineBreak ? 0 : 10)) * 3;

      write = common.mustCall((data) => {
        output += data;
      }, 6);

      resume() {}
      pause() {}
      end() {}
    }

    const fi = new FakeInput();
    const rli = new readline.Interface({
      input: fi,
      output: fi,
      terminal: true,
      completer: common.mustCallAtLeast(completer),
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

{
  let output = '';
  class FakeInput extends EventEmitter {
    columns = 80;

    write = common.mustCall((data) => {
      output += data;
    }, 1);

    resume() {}
    pause() {}
    end() {}
  }

  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    terminal: true,
    completer:
        common.mustCallAtLeast((_, cb) => cb(new Error('message'))),
  });

  rli.on('line', common.mustNotCall());
  fi.emit('data', '\t');
  queueMicrotask(common.mustCall(() => {
    assert.match(output, /^Tab completion error: Error: message/);
    output = '';
  }));
  rli.close();
}

{
  let output = '';
  class FakeInput extends EventEmitter {
    columns = 80;

    write = common.mustCall((data) => {
      output += data;
    }, 9);

    resume() {}
    pause() {}
    end() {}
  }

  const fi = new FakeInput();
  const rli = new readline.Interface({
    input: fi,
    output: fi,
    terminal: true,
    completer: common.mustCall((input, cb) => {
      cb(null, [[input[0].toUpperCase() + input.slice(1)], input]);
    }),
  });

  rli.on('line', common.mustNotCall());
  fi.emit('data', 'input');
  queueMicrotask(common.mustCall(() => {
    fi.emit('data', '\t');
    queueMicrotask(common.mustCall(() => {
      assert.match(output, /> Input/);
      output = '';
      rli.close();
    }));
  }));
}

{
	class VirtualScreen {
		constructor() {
			this.rows = [[]];
			this.row = 0;
			this.col = 0;
		}

		ensureRow(row) {
			while (this.rows.length <= row) this.rows.push([]);
		}

		setChar(row, col, ch) {
			this.ensureRow(row);
			const target = this.rows[row];
			while (target.length <= col) target.push(' ');
			target[col] = ch;
		}

		clearLineRight() {
			this.ensureRow(this.row);
			const target = this.rows[this.row];
			if (this.col < target.length) {
				target.length = this.col;
			}
		}

		clearFromCursor() {
			this.clearLineRight();
			if (this.row + 1 < this.rows.length) {
				this.rows.length = this.row + 1;
			}
		}

		moveCursor(dx, dy) {
			this.row = Math.max(0, this.row + dy);
			this.ensureRow(this.row);
			this.col = Math.max(0, this.col + dx);
		}

		handleEscape(params, code) {
			switch (code) {
				case 'A': // Cursor Up
					this.moveCursor(0, -(Number(params) || 1));
					break;
				case 'B': // Cursor Down
					this.moveCursor(0, Number(params) || 1);
					break;
				case 'C': // Cursor Forward
					this.moveCursor(Number(params) || 1, 0);
					break;
				case 'D': // Cursor Backward
					this.moveCursor(-(Number(params) || 1), 0);
					break;
				case 'G': // Cursor Horizontal Absolute
					this.col = Math.max(0, (Number(params) || 1) - 1);
					break;
				case 'H':
				case 'f': { // Cursor Position
					const [row, col] = params.split(';').map((n) => Number(n) || 1);
					this.row = Math.max(0, row - 1);
					this.col = Math.max(0, (col ?? 1) - 1);
					this.ensureRow(this.row);
					break;
				}
				case 'J':
					this.clearFromCursor();
					break;
				case 'K':
					this.clearLineRight();
					break;
				default:
					break;
			}
		}

		write(chunk) {
			for (let i = 0; i < chunk.length; i++) {
				const ch = chunk[i];
				if (ch === '\r') {
					this.col = 0;
					continue;
				}
				if (ch === '\n') {
					this.row++;
					this.col = 0;
					this.ensureRow(this.row);
					continue;
				}
				if (ch === '\u001b' && chunk[i + 1] === '[') {
					const match = /^\u001b\[([0-9;]*)([A-Za-z])/.exec(chunk.slice(i));
					if (match) {
						this.handleEscape(match[1], match[2]);
						i += match[0].length - 1;
						continue;
					}
				}
				this.setChar(this.row, this.col, ch);
				this.col++;
			}
		}

		getLines() {
			return this.rows.map((row) => row.join('').trimEnd());
		}
	}

	class FakeTTY extends EventEmitter {
		columns = 80;
		rows = 24;
		isTTY = true;

		constructor(screen) {
			super();
			this.screen = screen;
		}

		write(data) {
			this.screen.write(data);
			return true;
		}

		resume() {}

		pause() {}

		end() {}

		setRawMode(mode) {
			this.isRaw = mode;
		}
	}

	const screen = new VirtualScreen();
	const fi = new FakeTTY(screen);

	const rli = new readline.Interface({
		input: fi,
		output: fi,
		terminal: true,
		completer: (line) => [['foobar', 'foobaz'], line],
	});

	const promptLines = ['multiline', 'prompt', 'eats', 'output', '> '];
	rli.setPrompt(promptLines.join('\n'));
	rli.prompt();

	['f', 'o', 'o', '\t', '\t'].forEach((ch) => fi.emit('data', ch));

	const display = screen.getLines();

	assert.strictEqual(display[0], 'multiline');
	assert.strictEqual(display[1], 'prompt');
	assert.strictEqual(display[2], 'eats');
	assert.strictEqual(display[3], 'output');

	const inputLineIndex = 4;
	assert.ok(
		display[inputLineIndex].includes('> fooba'),
		'prompt line should keep completed input',
	);

	const completionLineExists =
		display.some((l) => l.includes('foobar') && l.includes('foobaz'));
	assert.ok(completionLineExists, 'completion list should be visible');

	rli.close();
}
