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
const CLEAR = { ctrl: true, name: 'u' };
const ESCAPE = { name: 'escape' };
const SEARCH = { ctrl: true, name: 'r' };

const prompt = '> ';
const reverseSearchPrompt = '(reverse-i-search)`\':';


const wrapWithSearchTexts = (code, result) => {
  return `(reverse-i-search)\`${code}': ${result}`;
};
const tests = [
  { // creates few history to search for
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: ['\' search\'.trim()', ENTER, 'let ab = 45', ENTER,
           '555 + 909', ENTER, '{key : {key2 :[] }}', ENTER],
    expected: [],
    clean: false
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: [SEARCH, 's', ESCAPE, CLEAR],
    expected: [reverseSearchPrompt,
               wrapWithSearchTexts('s', '\' search\'.trim()')]
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: ['s', SEARCH, ESCAPE, CLEAR],
    expected: [wrapWithSearchTexts('s', '\' search\'.trim()')]
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: ['5', SEARCH, SEARCH, ESCAPE, CLEAR],
    expected: [wrapWithSearchTexts('5', '555 + 909'),
               wrapWithSearchTexts('5', 'let ab = 45')]
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: ['*', SEARCH, ESCAPE, CLEAR],
    expected: ['(failed-reverse-i-search)`*\':'],
    clean: true
  }
];

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

const numtests = tests.length;

const runTestWrap = common.mustCall(runTest, numtests);

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

        if (!output.includes('reverse-i-search')) {
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
