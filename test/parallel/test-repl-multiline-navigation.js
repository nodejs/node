'use strict';

// Flags: --expose-internals

const common = require('../common');

const assert = require('assert');
const repl = require('internal/repl');
const stream = require('stream');
const fs = require('fs');

class ActionStream extends stream.Stream {
  run(data) {
    const _iter = data[Symbol.iterator]();
    const doAction = () => {
      const next = _iter.next();
      if (next.done) {
        // Close the repl. Note that it must have a clean prompt to do so.
        this.emit('keypress', '', { ctrl: true, name: 'd' });
        return;
      }
      const action = next.value;

      if (typeof action === 'object') {
        this.emit('keypress', '', action);
      }
      setImmediate(doAction);
    };
    doAction();
  }
  write(chunk) {
    const chunkLines = chunk.toString('utf8').split('\n');
    this.lines[this.lines.length - 1] += chunkLines[0];
    if (chunkLines.length > 1) {
      this.lines.push(...chunkLines.slice(1));
    }
    this.emit('line', this.lines[this.lines.length - 1]);
    return true;
  }
  resume() {}
  pause() {}
}
ActionStream.prototype.readable = true;

function cleanupTmpFile() {
  try {
    // Write over the file, clearing any history
    fs.writeFileSync(defaultHistoryPath, '');
  } catch (err) {
    if (err.code === 'ENOENT') return true;
    throw err;
  }
  return true;
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const defaultHistoryPath = tmpdir.resolve('.node_repl_history');

{
  cleanupTmpFile();
  // Make sure the cursor is at the right places.
  // If the cursor is at the end of a long line and the down key is pressed,
  // Move the cursor to the end of the next line, if shorter.
  const checkResults = common.mustSucceed((r) => {
    r.write('let str = `');
    r.input.run([{ name: 'enter' }]);
    r.write('111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222');
    r.input.run([{ name: 'enter' }]);
    r.write('3`');
    r.input.run([{ name: 'enter' }]);
    r.input.run([{ name: 'up' }]);
    assert.strictEqual(r.cursor, 33);

    r.input.run([{ name: 'up' }]);
    assert.strictEqual(r.cursor, 18);

    for (let i = 0; i < 5; i++) {
      r.input.run([{ name: 'right' }]);
    }
    r.input.run([{ name: 'up' }]);
    assert.strictEqual(r.cursor, 15);
    r.input.run([{ name: 'up' }]);

    for (let i = 0; i < 5; i++) {
      r.input.run([{ name: 'right' }]);
    }
    assert.strictEqual(r.cursor, 8);

    r.input.run([{ name: 'down' }]);
    assert.strictEqual(r.cursor, 15);

    r.input.run([{ name: 'down' }]);
    assert.strictEqual(r.cursor, 19);
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: defaultHistoryPath },
    {
      terminal: true,
      input: new ActionStream(),
      output: new stream.Writable({
        write(chunk, _, next) {
          next();
        }
      }),
    },
    checkResults
  );
}

{
  cleanupTmpFile();
  // If the last command errored and the user is trying to edit it,
  // The errored line should be removed from history
  const checkResults = common.mustSucceed((r) => {
    r.write('let lineWithMistake = `I have some');
    r.input.run([{ name: 'enter' }]);
    r.write('problem with` my syntax\'');
    r.input.run([{ name: 'enter' }]);
    r.input.run([{ name: 'up' }]);
    r.input.run([{ name: 'backspace' }]);
    r.write('`');
    for (let i = 0; i < 11; i++) {
      r.input.run([{ name: 'left' }]);
    }
    r.input.run([{ name: 'backspace' }]);
    r.input.run([{ name: 'enter' }]);

    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], 'let lineWithMistake = `I have some\rproblem with my syntax`');
    assert.strictEqual(r.line, '');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: defaultHistoryPath },
    {
      terminal: true,
      input: new ActionStream(),
      output: new stream.Writable({
        write(chunk, _, next) {
          next();
        }
      }),
    },
    checkResults
  );
}

{
  cleanupTmpFile();
  const outputBuffer = [];

  // Test that the REPL preview is properly shown on multiline commands
  // And deleted when enter is pressed
  const checkResults = common.mustSucceed((r) => {
    r.write('Array(100).fill(');
    r.input.run([{ name: 'enter' }]);
    r.write('123');
    r.input.run([{ name: 'enter' }]);
    r.write(')');
    r.input.run([{ name: 'enter' }]);
    r.input.run([{ name: 'up' }]);
    r.input.run([{ name: 'up' }]);

    assert.deepStrictEqual(r.last, new Array(100).fill(123));
    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(outputBuffer.includes('[\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,\n' +
    '  123\n' +
    ']\n'), true);
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: defaultHistoryPath },
    {
      preview: true,
      terminal: true,
      input: new ActionStream(),
      output: new stream.Writable({
        write(chunk, _, next) {
          // Store each chunk in the buffer
          outputBuffer.push(chunk.toString());
          next();
        }
      }),
    },
    checkResults
  );
}
