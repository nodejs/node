'use strict';

require('../common');
const assert = require('assert');
const { processTopLevelAwait } = require('internal/repl/await');

// Flags: --expose-internals

// This test was created based on
// https://cs.chromium.org/chromium/src/third_party/WebKit/LayoutTests/http/tests/inspector-unit/preprocess-top-level-awaits.js?rcl=358caaba5e763e71c4abb9ada2d9cd8b1188cac9

const testCases = [
  [ '0',
    null ],
  [ 'await 0',
    '(async () => {\nreturn (await 0)\n})()' ],
  [ 'await 0;',
    '(async () => {\nreturn (await 0);\n})()' ],
  [ '(await 0)',
    '(async () => {\nreturn ((await 0))\n})()' ],
  [ '(await 0);',
    '(async () => {\nreturn ((await 0));\n})()' ],
  [ 'async function foo() { await 0; }',
    null ],
  [ 'async () => await 0',
    null ],
  [ 'class A { async method() { await 0 } }',
    null ],
  [ 'await 0; return 0;',
    null ],
  [ 'var a = await 1',
    '(async () => {\nvoid (a = await 1)\n})()' ],
  [ 'let a = await 1',
    '(async () => {\nvoid (a = await 1)\n})()' ],
  [ 'const a = await 1',
    '(async () => {\nvoid (a = await 1)\n})()' ],
  [ 'for (var i = 0; i < 1; ++i) { await i }',
    '(async () => {\nfor (void (i = 0); i < 1; ++i) { await i }\n})()' ],
  [ 'for (let i = 0; i < 1; ++i) { await i }',
    '(async () => {\nfor (let i = 0; i < 1; ++i) { await i }\n})()' ],
  [ 'var {a} = {a:1}, [b] = [1], {c:{d}} = {c:{d: await 1}}',
    '(async () => {\nvoid ( ({a} = {a:1}), ([b] = [1]), ' +
        '({c:{d}} = {c:{d: await 1}}))\n})()' ],
  /* eslint-disable no-template-curly-in-string */
  [ 'console.log(`${(await { a: 1 }).a}`)',
    '(async () => {\nreturn (console.log(`${(await { a: 1 }).a}`))\n})()' ],
  /* eslint-enable no-template-curly-in-string */
  [ 'await 0; function foo() {}',
    '(async () => {\nawait 0; foo=function foo() {}\n})()' ],
  [ 'await 0; class Foo {}',
    '(async () => {\nawait 0; Foo=class Foo {}\n})()' ],
  [ 'if (await true) { function foo() {} }',
    '(async () => {\nif (await true) { foo=function foo() {} }\n})()' ],
  [ 'if (await true) { class Foo{} }',
    '(async () => {\nif (await true) { class Foo{} }\n})()' ],
  [ 'if (await true) { var a = 1; }',
    '(async () => {\nif (await true) { void (a = 1); }\n})()' ],
  [ 'if (await true) { let a = 1; }',
    '(async () => {\nif (await true) { let a = 1; }\n})()' ],
  [ 'var a = await 1; let b = 2; const c = 3;',
    '(async () => {\nvoid (a = await 1); void (b = 2); void (c = 3);\n})()' ],
  [ 'let o = await 1, p',
    '(async () => {\nvoid ( (o = await 1), (p=undefined))\n})()' ],
];

for (const [input, expected] of testCases) {
  assert.strictEqual(processTopLevelAwait(input), expected);
}
