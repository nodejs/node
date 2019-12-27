'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { inspect } = require('util');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const defaultHistoryPath = path.join(tmpdir.path, '.node_repl_history');

// Create an input stream specialized for testing an array of actions
class ActionStream extends stream.Stream {
  run(data) {
    let reallyWait = true;
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
      if (action === WAIT && reallyWait) {
        setTimeout(doAction, common.platformTimeout(50));
        reallyWait = false;
      } else {
        setImmediate(doAction);
      }
    };
    doAction();
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
const RIGHT = { name: 'right' };
const DELETE = { name: 'delete' };
const BACKSPACE = { name: 'backspace' };
const WORD_LEFT = { name: 'left', ctrl: true };
const WORD_RIGHT = { name: 'right', ctrl: true };
const GO_TO_END = { name: 'end' };

const prompt = '> ';
const WAIT = '€';

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
    checkTotal: true,
    test: [UP, UP, UP, UP, UP, DOWN, DOWN, DOWN, DOWN],
    expected: [prompt,
               `${prompt}Array(100).fill(1).map((e, i) => i ** 2)`,
               prev && '\n// [ 0, 1, 4, 9, 16, 25, 36, 49, 64, 81, 100, 121, ' +
                 '144, 169, 196, 225, 256, 289, 324, 361, 400, 441, 484, 529,' +
                 ' 576, 625, 676, 729, 784, 841, 900, 961, 1024, 1089, 1156, ' +
                 '1225, 1296, 1369, 1444, 1521, 1600, 1681, 1764, 1849, 1936,' +
                 ' 2025, 2116, 2209,...',
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
                 ' 2025, 2116, 2209,...',
               prompt].filter((e) => typeof e === 'string'),
    clean: false
  },
  { // Creates more history entries to navigate through.
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    test: [
      '555 + 909', ENTER, // Add a duplicate to the history set.
      'const foo = true', ENTER,
      '555n + 111n', ENTER,
      '5 + 5', ENTER,
      '55 - 13 === 42', ENTER
    ],
    expected: [],
    clean: false
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    checkTotal: true,
    preview: false,
    showEscapeCodes: true,
    test: [
      '55', UP, UP, UP, UP, UP, UP, ENTER
    ],
    expected: [
      '\x1B[1G', '\x1B[0J', prompt, '\x1B[3G',
      // '55'
      '5', '5',
      // UP
      '\x1B[1G', '\x1B[0J',
      '> 55 - 13 === 42', '\x1B[17G',
      // UP - skipping 5 + 5
      '\x1B[1G', '\x1B[0J',
      '> 555n + 111n', '\x1B[14G',
      // UP - skipping const foo = true
      '\x1B[1G', '\x1B[0J',
      '> 555 + 909', '\x1B[12G',
      // UP, UP, ENTER. UPs at the end of the history have no effect.
      '\r\n',
      '1464\n',
      '\x1B[1G', '\x1B[0J',
      '> ', '\x1B[3G',
      '\r\n'
    ],
    clean: true
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    skip: !process.features.inspector,
    test: [
      `const ${'veryLongName'.repeat(30)} = 'I should be previewed'`,
      ENTER,
      'const e = new RangeError("visible\\ninvisible")',
      ENTER,
      'e',
      ENTER,
      'veryLongName'.repeat(30),
      ENTER,
      `${'\x1B[90m \x1B[39m'.repeat(235)} fun`,
      ENTER,
      `${' '.repeat(236)} fun`,
      ENTER
    ],
    expected: [],
    clean: false
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    columns: 250,
    showEscapeCodes: true,
    skip: !process.features.inspector,
    test: [
      UP,
      UP,
      UP,
      WORD_LEFT,
      UP,
      BACKSPACE
    ],
    // A = Cursor n up
    // B = Cursor n down
    // C = Cursor n forward
    // D = Cursor n back
    // G = Cursor to column n
    // J = Erase in screen; 0 = right; 1 = left; 2 = total
    // K = Erase in line; 0 = right; 1 = left; 2 = total
    expected: [
      // 0. Start
      '\x1B[1G', '\x1B[0J',
      prompt, '\x1B[3G',
      // 1. UP
      // This exceeds the maximum columns (250):
      // Whitespace + prompt + ' // '.length + 'function'.length
      // 236 + 2 + 4 + 8
      '\x1B[1G', '\x1B[0J',
      `${prompt}${' '.repeat(236)} fun`, '\x1B[243G',
      // 2. UP
      '\x1B[1G', '\x1B[0J',
      `${prompt}${' '.repeat(235)} fun`, '\x1B[242G',
      // TODO(BridgeAR): Investigate why the preview is generated twice.
      ' // ction', '\x1B[242G',
      ' // ction', '\x1B[242G',
      // Preview cleanup
      '\x1B[0K',
      // 3. UP
      '\x1B[1G', '\x1B[0J',
      // 'veryLongName'.repeat(30).length === 360
      // prompt.length === 2
      // 360 % 250 + 2 === 112 (+1)
      `${prompt}${'veryLongName'.repeat(30)}`, '\x1B[113G',
      // "// 'I should be previewed'".length + 86 === 112 (+1)
      "\n// 'I should be previewed'", '\x1B[86C\x1B[1A',
      // Preview cleanup
      '\x1B[1B', '\x1B[2K', '\x1B[1A',
      // 4. WORD LEFT
      // Almost identical as above. Just one extra line.
      // Math.floor(360 / 250) === 1
      '\x1B[1A',
      '\x1B[1G', '\x1B[0J',
      `${prompt}${'veryLongName'.repeat(30)}`, '\x1B[3G', '\x1B[1A',
      '\x1B[1B', "\n// 'I should be previewed'", '\x1B[24D\x1B[2A',
      // Preview cleanup
      '\x1B[2B', '\x1B[2K', '\x1B[2A',
      // 5. UP
      '\x1B[1G', '\x1B[0J',
      `${prompt}e`, '\x1B[4G',
      // '// RangeError: visible'.length - 19 === 3 (+1)
      '\n// RangeError: visible', '\x1B[19D\x1B[1A',
      // Preview cleanup
      '\x1B[1B', '\x1B[2K', '\x1B[1A',
      // 6. Backspace
      '\x1B[1G', '\x1B[0J',
      prompt, '\x1B[3G', '\r\n'
    ],
    clean: true
  },
  {
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    showEscapeCodes: true,
    skip: !process.features.inspector,
    test: [
      'fun',
      RIGHT,
      BACKSPACE,
      LEFT,
      LEFT,
      'A',
      BACKSPACE,
      GO_TO_END,
      BACKSPACE,
      WORD_LEFT,
      WORD_RIGHT,
      ENTER
    ],
    // C = Cursor n forward
    // D = Cursor n back
    // G = Cursor to column n
    // J = Erase in screen; 0 = right; 1 = left; 2 = total
    // K = Erase in line; 0 = right; 1 = left; 2 = total
    expected: [
      // 0.
      // 'f'
      '\x1B[1G', '\x1B[0J', prompt, '\x1B[3G', 'f',
      // 'u'
      'u', ' // nction', '\x1B[5G',
      // 'n' - Cleanup
      '\x1B[0K',
      'n', ' // ction', '\x1B[6G',
      // 1. Right. Cleanup
      '\x1B[0K',
      'ction',
      // 2. Backspace. Refresh
      '\x1B[1G', '\x1B[0J', `${prompt}functio`, '\x1B[10G',
      // Autocomplete and refresh?
      ' // n', '\x1B[10G', ' // n', '\x1B[10G',
      // 3. Left. Cleanup
      '\x1B[0K',
      '\x1B[1D', '\x1B[10G', ' // n', '\x1B[9G',
      // 4. Left. Cleanup
      '\x1B[10G', '\x1B[0K', '\x1B[9G',
      '\x1B[1D', '\x1B[10G', ' // n', '\x1B[8G',
      // 5. 'A' - Cleanup
      '\x1B[10G', '\x1B[0K', '\x1B[8G',
      // Refresh
      '\x1B[1G', '\x1B[0J', `${prompt}functAio`, '\x1B[9G',
      // 6. Backspace. Refresh
      '\x1B[1G', '\x1B[0J', `${prompt}functio`, '\x1B[8G', '\x1B[10G', ' // n',
      '\x1B[8G', '\x1B[10G', ' // n',
      '\x1B[8G', '\x1B[10G',
      // 7. Go to end. Cleanup
      '\x1B[0K', '\x1B[8G', '\x1B[2C',
      'n',
      // 8. Backspace. Refresh
      '\x1B[1G', '\x1B[0J', `${prompt}functio`, '\x1B[10G',
      // Autocomplete
      ' // n', '\x1B[10G', ' // n', '\x1B[10G',
      // 9. Word left. Cleanup
      '\x1B[0K', '\x1B[7D', '\x1B[10G', ' // n', '\x1B[3G', '\x1B[10G',
      // 10. Word right. Cleanup
      '\x1B[0K', '\x1B[3G', '\x1B[7C', ' // n', '\x1B[10G',
      '\x1B[0K',
      // 11. ENTER
      '\r\n',
      'Uncaught ReferenceError: functio is not defined\n',
      '\x1B[1G', '\x1B[0J',
      prompt, '\x1B[3G', '\r\n'
    ],
    clean: true
  },
  {
    // Check that the completer ignores completions that are outdated.
    env: { NODE_REPL_HISTORY: defaultHistoryPath },
    completer(line, callback) {
      if (line.endsWith(WAIT)) {
        setTimeout(
          callback,
          common.platformTimeout(40),
          null,
          [[`${WAIT}WOW`], line]
        );
      } else {
        callback(null, [[' Always visible'], line]);
      }
    },
    skip: !process.features.inspector,
    test: [
      WAIT, // The first call is awaited before new input is triggered!
      BACKSPACE,
      's',
      BACKSPACE,
      WAIT, // The second call is not awaited. It won't trigger the preview.
      BACKSPACE,
      's',
      BACKSPACE
    ],
    expected: [
      prompt,
      WAIT,
      ' // WOW',
      prompt,
      's',
      ' // Always visible',
      prompt,
      WAIT,
      prompt,
      's',
      ' // Always visible',
      prompt,
    ],
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

  const { expected, skip } = opts;

  // Test unsupported on platform.
  if (skip) {
    setImmediate(runTestWrap, true);
    return;
  }
  const lastChunks = [];
  let i = 0;

  REPL.createInternalRepl(opts.env, {
    input: new ActionStream(),
    output: new stream.Writable({
      write(chunk, _, next) {
        const output = chunk.toString();

        if (!opts.showEscapeCodes &&
            (output[0] === '\x1B' || /^[\r\n]+$/.test(output))) {
          return next();
        }

        lastChunks.push(output);

        if (expected.length && !opts.checkTotal) {
          try {
            assert.strictEqual(output, expected[i]);
          } catch (e) {
            console.error(`Failed test # ${numtests - tests.length}`);
            console.error('Last outputs: ' + inspect(lastChunks, {
              breakLength: 5, colors: true
            }));
            throw e;
          }
          // TODO(BridgeAR): Auto close on last chunk!
          i++;
        }

        next();
      }
    }),
    completer: opts.completer,
    prompt,
    useColors: false,
    preview: opts.preview,
    terminal: true
  }, function(err, repl) {
    if (err) {
      console.error(`Failed test # ${numtests - tests.length}`);
      throw err;
    }

    repl.once('close', () => {
      if (opts.clean)
        cleanupTmpFile();

      if (opts.checkTotal) {
        assert.deepStrictEqual(lastChunks, expected);
      } else if (expected.length !== i) {
        console.error(tests[numtests - tests.length - 1]);
        throw new Error(`Failed test # ${numtests - tests.length}`);
      }

      setImmediate(runTestWrap, true);
    });

    if (opts.columns) {
      Object.defineProperty(repl, 'columns', {
        value: opts.columns,
        enumerable: true
      });
    }
    repl.inputStream.run(opts.test);
  });
}

// run the tests
runTest();
