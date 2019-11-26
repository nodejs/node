'use strict';

// Flags: --expose-internals

const common = require('../common');
const stream = require('stream');
const REPL = require('internal/repl');
const assert = require('assert');

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
        this.emit('data', `${action}\n`);
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

const prompt = '> ';


const wrapWithColorCode = (code, result) => {
  return `${prompt}${code}\u001b[90m // ${result}\u001b[39m`;
};
const tests = [
  {
    env: {},
    test: ['\' t\'.trim()', CLEAR],
    expected: [wrapWithColorCode('\' t\'', '\' t\''),
               wrapWithColorCode('\' t\'.trim', '[Function: trim]'),
               wrapWithColorCode('\' t\'.trim()', '\'t\'')]
  },
  {
    env: {},
    test: ['3+5', CLEAR],
    expected: [wrapWithColorCode('3', '3'),
               wrapWithColorCode('3+5', '8')]
  },
  {
    env: {},
    test: ['[9,0].sort()', CLEAR],
    expected: [wrapWithColorCode('[9,0]', '[ 9, 0 ]'),
               wrapWithColorCode('[9,0].sort', '[Function: sort]'),
               wrapWithColorCode('[9,0].sort()', '[ 0, 9 ]')]
  },
  {
    env: {},
    test: ['const obj = { m : () => {}}', ENTER,
           'obj.m', CLEAR],
    expected: [
      wrapWithColorCode('obj', '{ m: [Function: m] }'),
      wrapWithColorCode('obj.m', '[Function: m]')]
  },
  {
    env: {},
    test: ['const aObj = { a : { b : { c : [ {} , \'test\' ]}}}', ENTER,
           'aObj.a', CLEAR],
    expected: [
      wrapWithColorCode('aObj',
                        '{ a: { b: { c: [ {}, \'test\' ] } } }'),
      wrapWithColorCode('aObj.a',
                        '{ b: { c: [ {}, \'test\' ] } }')]
  }
];
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

        // Ignore everything except eval result
        if (!output.includes('//')) {
          return next();
        }

        const toBeAsserted = expected[0];
        try {
          assert.strictEqual(output, toBeAsserted);
          expected.shift();
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

    repl.once('close', () => {
      try {
        // Ensure everything that we expected was output
        assert.strictEqual(expected.length, 0);
        setImmediate(runTestWrap, true);
      } catch (err) {
        console.error(`Failed test # ${numtests - tests.length}`);
        throw err;
      }
    });

    repl.inputStream.run(test);
  });
}

// run the tests
runTest();
