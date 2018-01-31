'use strict';

const common = require('../common');
const assert = require('assert');
const { stripVTControlCharacters } = require('internal/readline');
const repl = require('repl');

common.crashOnUnhandledRejection();

// Flags: --expose-internals

const PROMPT = 'await repl > ';

class REPLStream extends common.ArrayStream {
  constructor() {
    super();
    this.waitingForResponse = false;
    this.lines = [''];
  }
  write(chunk, encoding, callback) {
    if (Buffer.isBuffer(chunk)) {
      chunk = chunk.toString(encoding);
    }
    const chunkLines = stripVTControlCharacters(chunk).split('\n');
    this.lines[this.lines.length - 1] += chunkLines[0];
    if (chunkLines.length > 1) {
      this.lines.push(...chunkLines.slice(1));
    }
    this.emit('line');
    if (callback) callback();
    return true;
  }

  wait(lookFor = PROMPT) {
    if (this.waitingForResponse) {
      throw new Error('Currently waiting for response to another command');
    }
    this.lines = [''];
    return new Promise((resolve, reject) => {
      const onError = (err) => {
        this.removeListener('line', onLine);
        reject(err);
      };
      const onLine = () => {
        if (this.lines[this.lines.length - 1].includes(lookFor)) {
          this.removeListener('error', onError);
          this.removeListener('line', onLine);
          resolve(this.lines);
        }
      };
      this.once('error', onError);
      this.on('line', onLine);
    });
  }
}

const putIn = new REPLStream();
const testMe = repl.start({
  prompt: PROMPT,
  stream: putIn,
  terminal: true,
  useColors: false,
  breakEvalOnSigint: true
});

function runAndWait(cmds, lookFor) {
  const promise = putIn.wait(lookFor);
  for (const cmd of cmds) {
    if (typeof cmd === 'string') {
      putIn.run([cmd]);
    } else {
      testMe.write('', cmd);
    }
  }
  return promise;
}

async function ordinaryTests() {
  // These tests were created based on
  // https://cs.chromium.org/chromium/src/third_party/WebKit/LayoutTests/http/tests/devtools/console/console-top-level-await.js?rcl=5d0ea979f0ba87655b7ef0e03b58fa3c04986ba6
  putIn.run([
    'function foo(x) { return x; }',
    'function koo() { return Promise.resolve(4); }'
  ]);
  const testCases = [
    [ 'await Promise.resolve(0)', '0' ],
    [ '{ a: await Promise.resolve(1) }', '{ a: 1 }' ],
    [ '_', '{ a: 1 }' ],
    [ 'let { a, b } = await Promise.resolve({ a: 1, b: 2 }), f = 5;',
      'undefined' ],
    [ 'a', '1' ],
    [ 'b', '2' ],
    [ 'f', '5' ],
    [ 'let c = await Promise.resolve(2)', 'undefined' ],
    [ 'c', '2' ],
    [ 'let d;', 'undefined' ],
    [ 'd', 'undefined' ],
    [ 'let [i, { abc: { k } }] = [0, { abc: { k: 1 } }];', 'undefined' ],
    [ 'i', '0' ],
    [ 'k', '1' ],
    [ 'var l = await Promise.resolve(2);', 'undefined' ],
    [ 'l', '2' ],
    [ 'foo(await koo())', '4' ],
    [ '_', '4' ],
    [ 'const m = foo(await koo());', 'undefined' ],
    [ 'm', '4' ],
    [ 'const n = foo(await\nkoo());', 'undefined' ],
    [ 'n', '4' ],
    // eslint-disable-next-line no-template-curly-in-string
    [ '`status: ${(await Promise.resolve({ status: 200 })).status}`',
      "'status: 200'"],
    [ 'for (let i = 0; i < 2; ++i) await i', 'undefined' ],
    [ 'for (let i = 0; i < 2; ++i) { await i }', 'undefined' ],
    [ 'await 0', '0' ],
    [ 'await 0; function foo() {}', 'undefined' ],
    [ 'foo', '[Function: foo]' ],
    [ 'class Foo {}; await 1;', '1' ],
    [ 'Foo', '[Function: Foo]' ],
    [ 'if (await true) { function bar() {}; }', 'undefined' ],
    [ 'bar', '[Function: bar]' ],
    [ 'if (await true) { class Bar {}; }', 'undefined' ],
    [ 'Bar', 'ReferenceError: Bar is not defined', { line: 0 } ],
    [ 'await 0; function* gen(){}', 'undefined' ],
    [ 'for (var i = 0; i < 10; ++i) { await i; }', 'undefined' ],
    [ 'i', '10' ],
    [ 'for (let j = 0; j < 5; ++j) { await j; }', 'undefined' ],
    [ 'j', 'ReferenceError: j is not defined', { line: 0 } ],
    [ 'gen', '[GeneratorFunction: gen]' ],
    [ 'return 42; await 5;', 'SyntaxError: Illegal return statement',
      { line: 3 } ],
    [ 'let o = await 1, p', 'undefined' ],
    [ 'p', 'undefined' ],
    [ 'let q = 1, s = await 2', 'undefined' ],
    [ 's', '2' ]
  ];

  for (const [input, expected, options = {}] of testCases) {
    console.log(`Testing ${input}`);
    const toBeRun = input.split('\n');
    const lines = await runAndWait(toBeRun);
    if ('line' in options) {
      assert.strictEqual(lines[toBeRun.length + options.line], expected);
    } else {
      const echoed = toBeRun.map((a, i) => `${i > 0 ? '... ' : ''}${a}\r`);
      assert.deepStrictEqual(lines, [...echoed, expected, PROMPT]);
    }
  }
}

async function ctrlCTest() {
  putIn.run([
    `const timeout = (msecs) => new Promise((resolve) => {
       setTimeout(resolve, msecs).unref();
     });`
  ]);

  console.log('Testing Ctrl+C');
  assert.deepStrictEqual(await runAndWait([
    'await timeout(100000)',
    { ctrl: true, name: 'c' }
  ]), [
    'await timeout(100000)\r',
    'Thrown: Error [ERR_SCRIPT_EXECUTION_INTERRUPTED]: ' +
      'Script execution was interrupted by `SIGINT`.',
    PROMPT
  ]);
}

async function main() {
  await ordinaryTests();
  await ctrlCTest();
}

main();
