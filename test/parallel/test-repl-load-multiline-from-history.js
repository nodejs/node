'use strict';

// Flags: --expose-internals

const common = require('../common');

const assert = require('assert');
const repl = require('internal/repl');
const stream = require('stream');
const fixtures = require('../common/fixtures');

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

{
  // Testing multiline history loading
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const replHistoryPath = fixtures.path('.node_repl_history_multiline');

  const checkResults = common.mustSucceed((r) => {
    assert.strictEqual(r.history.length, 4);

    r.input.run([{ name: 'up' }]);
    assert.strictEqual(r.line, 'var d = [\n' +
      '  {\n' +
      '    a: 1,\n' +
      '    b: 2,\n' +
      '  },\n' +
      '  {\n' +
      '    a: 3,\n' +
      '    b: 4,\n' +
      '    c: [{ a: 1, b: 2 },\n' +
      '      {\n' +
      '        a: 3,\n' +
      '        b: 4,\n' +
      '      }\n' +
      '    ]\n' +
      '  }\n' +
      ']'
    );

    // Move the cursor all lines up until the former entry is retrieved.
    for (let i = 0; i < r.line.split('\n').length; i++) {
      r.input.run([{ name: 'up' }]);
    }
    assert.strictEqual(r.line, 'const c = [\n  {\n    a: 1,\n    b: 2,\n  }\n]');

    // Move the cursor all lines up until the former entry is retrieved.
    for (let i = 0; i < r.line.split('\n').length; i++) {
      r.input.run([{ name: 'up' }]);
    }
    assert.strictEqual(r.line, '`const b = [\n  1,\n  2,\n  3,\n  4,\n]`');

    // Move the cursor all lines up until the former entry is retrieved.
    for (let i = 0; i < r.line.split('\n').length; i++) {
      r.input.run([{ name: 'up' }]);
    }
    assert.strictEqual(r.line, 'a = `\nI am a multiline string\nI can be as long as I want`');
  });

  repl.createInternalRepl(
    { NODE_REPL_HISTORY: replHistoryPath },
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
