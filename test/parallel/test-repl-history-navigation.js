'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const defaultHistoryPath = path.join(tmpdir.path, '.node_repl_history');

// Create an input stream specialized for testing an array of actions
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
      } else {
        this.emit('data', `${action}`);
      }
      setImmediate(doAction);
    };
    setImmediate(doAction);
  }
  resume() {}
  pause() {}
}
ActionStream.prototype.readable = true;


// Mock keys
const ENTER = { name: 'enter' };
const UP = { name: 'up' };
const DOWN = { name: 'down' };
const LEFT = { name: 'left' };
const DELETE = { name: 'delete' };

const prompt = '> ';

const prev = process.features.inspector;

const tests = [
  { // Creates few history to navigate for
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: [ 'let ab = 45', ENTER,
            '555 + 909', ENTER,
            '{key : {key2 :[] }}', ENTER,
            'Array(100).fill(1).map((e, i) => i ** i)', LEFT, LEFT, DELETE,
            '2', ENTER],
    expected: [],
    clean: false
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: [UP, UP, UP, UP, UP, DOWN, DOWN, DOWN, DOWN],
    expected: [prompt,
               `${prompt}Array(100).fill(1).map((e, i) => i ** 2)`,
               prev && '\n// [ 0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, ' +
                 '144, 169, 196, 225, 256, 289, 324, 361, 400, 441, 484, 529,' +
                 ' 576, 625, 676, 729, 784, 841, 900, 961, 1024, 1089, 1156, ' +
                 '1225, 1296, 1369, 1444, 1521, 1600, 1681, 1764, 1849, 1936,' +
                 ' 2025, 2116, 2209, ...',
               `${prompt}{key : {key2 :[] }}`,
               prev && '\n// { key: { key2: [] } }',
               `${prompt}555 + 909`,
               prev && '\n// 1464',
               `${prompt}let ab = 45`,
               `${prompt}555 + 909`,
               prev && '\n// 1464',
               `${prompt}{key : {key2 :[] }}`,
               prev && '\n// { key: { key2: [] } }',
               `${prompt}Array(100).fill(1).map((e, i) => i ** 2)`,
               prev && '\n// [ 0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, ' +
                 '144, 169, 196, 225, 256, 289, 324, 361, 400, 441, 484, 529,' +
                 ' 576, 625, 676, 729, 784, 841, 900, 961, 1024, 1089, 1156, ' +
                 '1225, 1296, 1369, 1444, 1521, 1600, 1681, 1764, 1849, 1936,' +
                 ' 2025, 2116, 2209, ...',
               prompt].filter((e) => typeof e === 'string'),
    clean: true
  }
];
const numtests = tests.length;

const runTestWrap = common.mustCall(runTest, numtests);

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

function runTest() {
  const opts = tests.shift();
  if (!opts) return; // All done

  const env = opts.env;
  const test = opts.test;
  const expected = opts.expected;

  REPL.createInternalRepl(env, {
    input: new ActionStream(),
    output: new stream.Writable({
      write(chunk, _, next) {
        const output = chunk.toString();

        if (output.charCodeAt(0) === 27 || /^[\r\n]+$/.test(output)) {
          return next();
        }

        if (expected.length) {
          assert.strictEqual(output, expected[0]);
          expected.shift();
        }

        next();
      }
    }),
    prompt: prompt,
    useColors: false,
    terminal: true
  }, function(err, repl) {
    if (err) {
      console.error(`Failed test # ${numtests - tests.length}`);
      throw err;
    }

    repl.once('close', () => {
      if (opts.clean)
        cleanupTmpFile();

      if (expected.length !== 0) {
        throw new Error(`Failed test # ${numtests - tests.length}`);
      }
      setImmediate(runTestWrap, true);
    });

    repl.inputStream.run(test);
  });
}

// run the tests
runTest();
