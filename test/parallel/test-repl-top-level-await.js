'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const { stripVTControlCharacters } = require('internal/readline/utils');
const repl = require('repl');

common.skipIfInspectorDisabled();

// Flags: --expose-internals --experimental-repl-await

const PROMPT = 'await repl > ';

class REPLStream extends ArrayStream {
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
    [ '_', '// { a: 1 }\r', { line: 0 } ],
    [ 'let { aa, bb } = await Promise.resolve({ aa: 1, bb: 2 }), f = 5;',
      'undefined' ],
    [ 'aa', ['// 1\r', '1'] ],
    [ 'bb', ['// 2\r', '2'] ],
    [ 'f', ['// 5\r', '5'] ],
    [ 'let cc = await Promise.resolve(2)', 'undefined' ],
    [ 'cc', ['// 2\r', '2'] ],
    [ 'let dd;', 'undefined' ],
    [ 'dd', 'undefined' ],
    [ 'let [ii, { abc: { kk } }] = [0, { abc: { kk: 1 } }];', 'undefined' ],
    [ 'ii', ['// 0\r', '0'] ],
    [ 'kk', ['// 1\r', '1'] ],
    [ 'var ll = await Promise.resolve(2);', 'undefined' ],
    [ 'll', ['// 2\r', '2'] ],
    [ 'foo(await koo())',
      [ 'f', '// 5oo', '// [Function: foo](await koo())\r', '4' ] ],
    [ '_', ['// 4\r', '4'] ],
    [ 'const m = foo(await koo());',
      [ 'const m = foo(await koo());\r', 'undefined' ] ],
    [ 'm', ['// 4\r', '4' ] ],
    [ 'const n = foo(await\nkoo());',
      [ 'const n = foo(await\r', '... koo());\r', 'undefined' ] ],
    [ 'n', ['// 4\r', '4' ] ],
    // eslint-disable-next-line no-template-curly-in-string
    [ '`status: ${(await Promise.resolve({ status: 200 })).status}`',
      "'status: 200'"],
    [ 'for (let i = 0; i < 2; ++i) await i',
      ['f', '// 5or (let i = 0; i < 2; ++i) await i\r', 'undefined'] ],
    [ 'for (let i = 0; i < 2; ++i) { await i }',
      [ 'f', '// 5or (let i = 0; i < 2; ++i) { await i }\r', 'undefined' ] ],
    [ 'await 0', ['await 0\r', '0'] ],
    [ 'await 0; function foo() {}',
      ['await 0; function foo() {}\r', 'undefined'] ],
    [ 'foo',
      ['f', '// 5oo', '// [Function: foo]\r', '[Function: foo]'] ],
    [ 'class Foo {}; await 1;', ['class Foo {}; await 1;\r', '1'] ],
    [ 'Foo', ['// [Function: Foo]\r', '[Function: Foo]'] ],
    [ 'if (await true) { function bar() {}; }',
      ['if (await true) { function bar() {}; }\r', 'undefined'] ],
    [ 'bar', ['// [Function: bar]\r', '[Function: bar]'] ],
    [ 'if (await true) { class Bar {}; }', 'undefined' ],
    [ 'Bar', 'Uncaught ReferenceError: Bar is not defined' ],
    [ 'await 0; function* gen(){}', 'undefined' ],
    [ 'for (var i = 0; i < 10; ++i) { await i; }',
      ['f', '// 5or (var i = 0; i < 10; ++i) { await i; }\r', 'undefined'] ],
    [ 'i', ['// 10\r', '10'] ],
    [ 'for (let j = 0; j < 5; ++j) { await j; }',
      ['f', '// 5or (let j = 0; j < 5; ++j) { await j; }\r', 'undefined'] ],
    [ 'j', 'Uncaught ReferenceError: j is not defined', { line: 0 } ],
    [ 'gen', ['// [GeneratorFunction: gen]\r', '[GeneratorFunction: gen]'] ],
    [ 'return 42; await 5;', 'Uncaught SyntaxError: Illegal return statement',
      { line: 3 } ],
    [ 'let o = await 1, p', 'undefined' ],
    [ 'p', 'undefined' ],
    [ 'let q = 1, s = await 2', 'undefined' ],
    [ 's', ['// 2\r', '2'] ],
    [ 'for await (let i of [1,2,3]) console.log(i)',
      [
        'f',
        '// 5or await (let i of [1,2,3]) console.log(i)\r',
        '1',
        '2',
        '3',
        'undefined'
      ]
    ]
  ];

  for (const [input, expected, options = {}] of testCases) {
    console.log(`Testing ${input}`);
    const toBeRun = input.split('\n');
    const lines = await runAndWait(toBeRun);
    if (Array.isArray(expected)) {
      if (lines[0] === input)
        lines.shift();
      assert.deepStrictEqual(lines, [...expected, PROMPT]);
    } else if ('line' in options) {
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
    'Uncaught:',
    '[Error [ERR_SCRIPT_EXECUTION_INTERRUPTED]: ' +
      'Script execution was interrupted by `SIGINT`] {',
    "  code: 'ERR_SCRIPT_EXECUTION_INTERRUPTED'",
    '}',
    PROMPT
  ]);
}

async function main() {
  await ordinaryTests();
  await ctrlCTest();
}

main();
