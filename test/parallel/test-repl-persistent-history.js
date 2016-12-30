'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const os = require('os');

common.refreshTmpDir();

// Mock os.homedir()
os.homedir = function() {
  return common.tmpDir;
};

// Create an input stream specialized for testing an array of actions
class ActionStream extends stream.Stream {
  run(data) {
    const _iter = data[Symbol.iterator]();
    const doAction = () => {
      const next = _iter.next();
      if (next.done) {
        // Close the repl. Note that it must have a clean prompt to do so.
        setImmediate(() => {
          this.emit('keypress', '', { ctrl: true, name: 'd' });
        });
        return;
      }
      const action = next.value;

      if (typeof action === 'object') {
        this.emit('keypress', '', action);
      } else {
        this.emit('data', action + '\n');
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
const UP = { name: 'up' };
const ENTER = { name: 'enter' };
const CLEAR = { ctrl: true, name: 'u' };
// Common message bits
const prompt = '> ';
const replDisabled = '\nPersistent history support disabled. Set the ' +
                     'NODE_REPL_HISTORY environment\nvariable to a valid, ' +
                     'user-writable path to enable.\n';
const convertMsg = '\nConverting old JSON repl history to line-separated ' +
                   'history.\nThe new repl history file can be found at ' +
                   path.join(common.tmpDir, '.node_repl_history') + '.\n';
const homedirErr = '\nError: Could not get the home directory.\n' +
                   'REPL session history will not be persisted.\n';
const replFailedRead = '\nError: Could not open history file.\n' +
                       'REPL session history will not be persisted.\n';
const sameHistoryFilePaths = '\nThe old repl history file has the same name ' +
                             'and location as the new one i.e., ' +
                             path.join(common.tmpDir, '.node_repl_history') +
                             ' and is empty.\nUsing it as is.\n';
// File paths
const fixtures = common.fixturesDir;
const historyFixturePath = path.join(fixtures, '.node_repl_history');
const historyPath = path.join(common.tmpDir, '.fixture_copy_repl_history');
const historyPathFail = path.join(common.tmpDir, '.node_repl\u0000_history');
const oldHistoryPath = path.join(fixtures, 'old-repl-history-file.json');
const enoentHistoryPath = path.join(fixtures, 'enoent-repl-history-file.json');
const emptyHistoryPath = path.join(fixtures, '.empty-repl-history-file');
const defaultHistoryPath = path.join(common.tmpDir, '.node_repl_history');

const tests = [
  {
    env: { NODE_REPL_HISTORY: '' },
    test: [UP],
    expected: [prompt, replDisabled, prompt]
  },
  {
    env: { NODE_REPL_HISTORY: ' ' },
    test: [UP],
    expected: [prompt, replDisabled, prompt]
  },
  {
    env: { NODE_REPL_HISTORY: '',
           NODE_REPL_HISTORY_FILE: enoentHistoryPath },
    test: [UP],
    expected: [prompt, replDisabled, prompt]
  },
  {
    env: { NODE_REPL_HISTORY: '',
           NODE_REPL_HISTORY_FILE: oldHistoryPath },
    test: [UP],
    expected: [prompt, replDisabled, prompt]
  },
  {
    env: { NODE_REPL_HISTORY_FILE: emptyHistoryPath },
    test: [UP],
    expected: [prompt, convertMsg, prompt]
  },
  {
    env: { NODE_REPL_HISTORY_FILE: defaultHistoryPath },
    test: [UP],
    expected: [prompt, sameHistoryFilePaths, prompt]
  },
  {
    env: { NODE_REPL_HISTORY: historyPath },
    test: [UP, CLEAR],
    expected: [prompt, prompt + '\'you look fabulous today\'', prompt]
  },
  {
    env: { NODE_REPL_HISTORY: historyPath,
           NODE_REPL_HISTORY_FILE: oldHistoryPath },
    test: [UP, CLEAR],
    expected: [prompt, prompt + '\'you look fabulous today\'', prompt]
  },
  {
    env: { NODE_REPL_HISTORY: historyPath,
           NODE_REPL_HISTORY_FILE: '' },
    test: [UP, CLEAR],
    expected: [prompt, prompt + '\'you look fabulous today\'', prompt]
  },
  {
    env: {},
    test: [UP],
    expected: [prompt]
  },
  {
    env: { NODE_REPL_HISTORY_FILE: oldHistoryPath },
    test: [UP, CLEAR, '\'42\'', ENTER],
    expected: [prompt, convertMsg, prompt, prompt + '\'=^.^=\'', prompt, '\'',
               '4', '2', '\'', '\'42\'\n', prompt, prompt],
    clean: false
  },
  { // Requires the above testcase
    env: {},
    test: [UP, UP, ENTER],
    expected: [prompt, prompt + '\'42\'', prompt + '\'=^.^=\'', '\'=^.^=\'\n',
               prompt]
  },
  {
    env: { NODE_REPL_HISTORY: historyPath,
           NODE_REPL_HISTORY_SIZE: 1 },
    test: [UP, UP, CLEAR],
    expected: [prompt, prompt + '\'you look fabulous today\'', prompt]
  },
  {
    env: { NODE_REPL_HISTORY_FILE: oldHistoryPath,
           NODE_REPL_HISTORY_SIZE: 1 },
    test: [UP, UP, UP, CLEAR],
    expected: [prompt, convertMsg, prompt, prompt + '\'=^.^=\'', prompt]
  },
  {
    env: { NODE_REPL_HISTORY: historyPathFail,
           NODE_REPL_HISTORY_SIZE: 1 },
    test: [UP],
    expected: [prompt, replFailedRead, prompt, replDisabled, prompt]
  },
  { // Make sure this is always the last test, since we change os.homedir()
    before: function before() {
      // Mock os.homedir() failure
      os.homedir = function() {
        throw new Error('os.homedir() failure');
      };
    },
    env: {},
    test: [UP],
    expected: [prompt, homedirErr, prompt, replDisabled, prompt]
  }
];
const numtests = tests.length;


var testsNotRan = tests.length;

process.on('beforeExit', () =>
  assert.strictEqual(testsNotRan, 0)
);

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

// Copy our fixture to the tmp directory
fs.createReadStream(historyFixturePath)
  .pipe(fs.createWriteStream(historyPath)).on('unpipe', () => runTest());

function runTest(assertCleaned) {
  const opts = tests.shift();
  if (!opts) return; // All done

  if (assertCleaned) {
    try {
      assert.strictEqual(fs.readFileSync(defaultHistoryPath, 'utf8'), '');
    } catch (e) {
      if (e.code !== 'ENOENT') {
        console.error(`Failed test # ${numtests - tests.length}`);
        throw e;
      }
    }
  }

  const env = opts.env;
  const test = opts.test;
  const expected = opts.expected;
  const clean = opts.clean;
  const before = opts.before;

  if (before) before();

  REPL.createInternalRepl(env, {
    input: new ActionStream(),
    output: new stream.Writable({
      write(chunk, _, next) {
        const output = chunk.toString();

        // Ignore escapes and blank lines
        if (output.charCodeAt(0) === 27 || /^[\r\n]+$/.test(output))
          return next();

        try {
          assert.strictEqual(output, expected.shift());
        } catch (err) {
          console.error(`Failed test # ${numtests - tests.length}`);
          throw err;
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

    // The REPL registers 'module' and 'require' globals
    common.allowGlobals(repl.context.module, repl.context.require);

    repl.once('close', () => {
      if (repl._flushing) {
        repl.once('flushHistory', onClose);
        return;
      }

      onClose();
    });

    function onClose() {
      const cleaned = clean === false ? false : cleanupTmpFile();

      try {
        // Ensure everything that we expected was output
        assert.strictEqual(expected.length, 0);
        testsNotRan--;
        setImmediate(runTest, cleaned);
      } catch (err) {
        console.error(`Failed test # ${numtests - tests.length}`);
        throw err;
      }
    }

    repl.inputStream.run(test);
  });
}
