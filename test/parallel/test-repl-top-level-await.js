'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const { stripVTControlCharacters } = require('internal/util/inspect');
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

  wait() {
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
        if (this.lines[this.lines.length - 1].includes(PROMPT)) {
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
  useColors: true,
  breakEvalOnSigint: true
});

function runAndWait(cmds) {
  const promise = putIn.wait();
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
    ['await Promise.resolve(0)', '0'],
    ['{ a: await Promise.resolve(1) }', '{ a: 1 }'],
    ['_', '{ a: 1 }'],
    ['let { aa, bb } = await Promise.resolve({ aa: 1, bb: 2 }), f = 5;'],
    ['aa', '1'],
    ['bb', '2'],
    ['f', '5'],
    ['let cc = await Promise.resolve(2)'],
    ['cc', '2'],
    ['let dd;'],
    ['dd'],
    ['let [ii, { abc: { kk } }] = [0, { abc: { kk: 1 } }];'],
    ['ii', '0'],
    ['kk', '1'],
    ['var ll = await Promise.resolve(2);'],
    ['ll', '2'],
    ['foo(await koo())', '4'],
    ['_', '4'],
    ['const m = foo(await koo());'],
    ['m', '4'],
    ['const n = foo(await\nkoo());',
     ['const n = foo(await\r', '... koo());\r', 'undefined']],
    ['n', '4'],
    // eslint-disable-next-line no-template-curly-in-string
    ['`status: ${(await Promise.resolve({ status: 200 })).status}`',
     "'status: 200'"],
    ['for (let i = 0; i < 2; ++i) await i'],
    ['for (let i = 0; i < 2; ++i) { await i }'],
    ['await 0', '0'],
    ['await 0; function foo() {}'],
    ['foo', '[Function: foo]'],
    ['class Foo {}; await 1;', '1'],
    ['Foo', '[Function: Foo]'],
    ['if (await true) { function bar() {}; }'],
    ['bar', '[Function: bar]'],
    ['if (await true) { class Bar {}; }'],
    ['Bar', 'Uncaught ReferenceError: Bar is not defined'],
    ['await 0; function* gen(){}'],
    ['for (var i = 0; i < 10; ++i) { await i; }'],
    ['i', '10'],
    ['for (let j = 0; j < 5; ++j) { await j; }'],
    ['j', 'Uncaught ReferenceError: j is not defined', { line: 0 }],
    ['gen', '[GeneratorFunction: gen]'],
    ['return 42; await 5;', 'Uncaught SyntaxError: Illegal return statement',
     { line: 3 }],
    ['let o = await 1, p'],
    ['p'],
    ['let q = 1, s = await 2'],
    ['s', '2'],
    ['for await (let i of [1,2,3]) console.log(i)',
     [
       'for await (let i of [1,2,3]) console.log(i)\r',
       '1',
       '2',
       '3',
       'undefined'
     ]
    ]
  ];

  for (const [input, expected = [`${input}\r`], options = {}] of testCases) {
    console.log(`Testing ${input}`);
    const toBeRun = input.split('\n');
    const lines = await runAndWait(toBeRun);
    if (Array.isArray(expected)) {
      if (expected.length === 1)
        expected.push('undefined');
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
