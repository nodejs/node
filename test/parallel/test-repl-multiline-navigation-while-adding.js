'use strict';

// Flags: --expose-internals

const common = require('../common');

const assert = require('assert');
const repl = require('internal/repl');
const stream = require('stream');

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

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  const historyPath = tmpdir.resolve(`.${Math.floor(Math.random() * 10000)}`);
  // Make sure the cursor is at the right places when pressing enter at the end of the first line.
  const checkResults = common.mustSucceed((r) => {
    r.write('let aaa = `I am a');
    r.input.run([{ name: 'enter' }]);
    r.write('1111111111111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222'); // The command is not complete yet. I can still edit it
    r.input.run([{ name: 'up' }]);
    r.input.run([{ name: 'up' }]); // I am on the first line
    for (let i = 0; i < 4; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the first line
    assert.strictEqual(r.cursor, 17);

    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(r.cursor, 18);
    assert.strictEqual(r.line, 'let aaa = `I am a\n\n1111111111111\n22222222222222');
    r.write('000');
    r.input.run([{ name: 'down' }]);
    r.input.run([{ name: 'down' }]); // I am in the last line
    for (let i = 0; i < 5; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the last line
    r.write('`'); // Making the command complete
    r.input.run([{ name: 'enter' }]); // Issuing it
    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], '22222222`222222\r1111111111111\r000\rlet aaa = `I am a');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: historyPath },
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
  const historyPath = tmpdir.resolve(`.${Math.floor(Math.random() * 10000)}`);
  // Make sure the cursor is at the right places when pressing enter in the middle of the first line.
  const checkResults = common.mustSucceed((r) => {
    r.write('let bbb = `I am a');
    r.input.run([{ name: 'enter' }]);
    r.write('1111111111111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222'); // The command is not complete yet. I can still edit it
    r.input.run([{ name: 'up' }]);
    r.input.run([{ name: 'up' }]); // I am on the first line
    for (let i = 0; i < 2; i++) {
      r.input.run([{ name: 'left' }]);
    } // I am right after the string definition
    assert.strictEqual(r.cursor, 11);

    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(r.cursor, 12);
    assert.strictEqual(r.line, 'let bbb = `\nI am a\n1111111111111\n22222222222222');
    r.write('000');
    r.input.run([{ name: 'enter' }]);
    r.input.run([{ name: 'down' }]);
    r.input.run([{ name: 'down' }]); // I am in the last line
    for (let i = 0; i < 14; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the last line
    r.write('`'); // Making the command complete
    r.input.run([{ name: 'enter' }]); // Issuing it
    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], '22222222222222`\r1111111111111\rI am a\r000\rlet bbb = `');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: historyPath },
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
  const historyPath = tmpdir.resolve(`.${Math.floor(Math.random() * 10000)}`);
  // Make sure the cursor is at the right places when pressing enter at the end of the second line.
  const checkResults = common.mustSucceed((r) => {
    r.write('let ccc = `I am a');
    r.input.run([{ name: 'enter' }]);
    r.write('1111111111111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222'); // The command is not complete yet. I can still edit it
    r.input.run([{ name: 'up' }]); // I am the end of second line
    assert.strictEqual(r.cursor, 31);

    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(r.cursor, 32);
    assert.strictEqual(r.line, 'let ccc = `I am a\n1111111111111\n\n22222222222222');
    r.write('000');
    r.input.run([{ name: 'down' }]); // I am in the last line
    for (let i = 0; i < 11; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the last line
    r.write('`'); // Making the command complete
    r.input.run([{ name: 'enter' }]); // Issuing it
    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], '22222222222222`\r000\r1111111111111\rlet ccc = `I am a');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: historyPath },
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
  const historyPath = tmpdir.resolve(`.${Math.floor(Math.random() * 10000)}`);
  // Make sure the cursor is at the right places when pressing enter in the middle of the second line.
  const checkResults = common.mustSucceed((r) => {
    r.write('let ddd = `I am a');
    r.input.run([{ name: 'enter' }]);
    r.write('1111111111111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222'); // The command is not complete yet. I can still edit it
    r.input.run([{ name: 'up' }]); // I am the end of second line
    assert.strictEqual(r.cursor, 31);

    for (let i = 0; i < 6; i++) {
      r.input.run([{ name: 'left' }]);
    } // I am in the middle of the second line

    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(r.cursor, 26);
    assert.strictEqual(r.line, 'let ddd = `I am a\n1111111\n111111\n22222222222222');
    r.input.run([{ name: 'down' }]); // I am at the beginning of the last line
    for (let i = 0; i < 14; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the last line
    r.write('`'); // Making the command complete
    r.input.run([{ name: 'enter' }]); // Issuing it
    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], '22222222222222`\r111111\r1111111\rlet ddd = `I am a');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: historyPath },
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
  const historyPath = tmpdir.resolve(`.${Math.floor(Math.random() * 10000)}`);
  // Make sure the cursor is at the right places when pressing enter at the beginning of the third line.
  const checkResults = common.mustSucceed((r) => {
    r.write('let eee = `I am a');
    r.input.run([{ name: 'enter' }]);
    r.write('1111111111111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222'); // The command is not complete yet. I can still edit it
    for (let i = 0; i < 14; i++) {
      r.input.run([{ name: 'left' }]);
    } // I am at the beginning of the last line
    assert.strictEqual(r.cursor, 32);
    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(r.cursor, 33);
    assert.strictEqual(r.line, 'let eee = `I am a\n1111111111111\n\n22222222222222');
    r.input.run([{ name: 'up' }]); // I am the beginning of the new line
    r.write('000');
    assert.strictEqual(r.cursor, 35);
    r.input.run([{ name: 'down' }]); // I am in the last line
    for (let i = 0; i < 11; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the last line
    r.write('`'); // Making the command complete
    r.input.run([{ name: 'enter' }]); // Issuing it
    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], '22222222222222`\r000\r1111111111111\rlet eee = `I am a');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: historyPath },
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
  const historyPath = tmpdir.resolve(`.${Math.floor(Math.random() * 10000)}`);
  // Make sure the cursor is at the right places when pressing enter in the middle of the third line
  // And executing the command while still in the middle of the multiline command
  const checkResults = common.mustSucceed((r) => {
    r.write('let fff = `I am a');
    r.input.run([{ name: 'enter' }]);
    r.write('1111111111111');
    r.input.run([{ name: 'enter' }]);
    r.write('22222222222222'); // The command is not complete yet. I can still edit it
    assert.strictEqual(r.cursor, 46);

    for (let i = 0; i < 6; i++) {
      r.input.run([{ name: 'left' }]);
    } // I am in the middle of the third line

    r.input.run([{ name: 'enter' }]);
    assert.strictEqual(r.cursor, 41);
    assert.strictEqual(r.line, 'let fff = `I am a\n1111111111111\n22222222\n222222');
    r.input.run([{ name: 'down' }]); // I am at the beginning of the last line
    for (let i = 0; i < 6; i++) {
      r.input.run([{ name: 'right' }]);
    } // I am at the end of the last line
    r.write('`'); // Making the command complete
    r.input.run([{ name: 'up' }]); // I am not at the end of the last line
    r.input.run([{ name: 'enter' }]); // Issuing the command
    assert.strictEqual(r.history.length, 1);
    assert.strictEqual(r.history[0], '222222`\r22222222\r1111111111111\rlet fff = `I am a');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: historyPath },
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
