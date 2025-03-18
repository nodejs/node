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
  // It moves the cursor at the right places
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

    r.input.run([{ name: 'right' }]);
    r.input.run([{ name: 'right' }]);
    r.input.run([{ name: 'right' }]);
    r.input.run([{ name: 'right' }]);
    r.input.run([{ name: 'right' }]);
    r.input.run([{ name: 'up' }]);
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
