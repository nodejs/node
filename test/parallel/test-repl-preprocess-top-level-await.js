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
    '(async () => { return (await 0) })()' ],
  [ 'await 0;',
    '(async () => { return (await 0); })()' ],
  [ '(await 0)',
    '(async () => { return ((await 0)) })()' ],
  [ '(await 0);',
    '(async () => { return ((await 0)); })()' ],
  [ 'async function foo() { await 0; }',
    null ],
  [ 'async () => await 0',
    null ],
  [ 'class A { async method() { await 0 } }',
    null ],
  [ 'await 0; return 0;',
    null ],
  [ 'var a = await 1',
    '(async () => { void (a = await 1) })()' ],
  [ 'let a = await 1',
    '(async () => { void (a = await 1) })()' ],
  [ 'const a = await 1',
    '(async () => { void (a = await 1) })()' ],
  [ 'for (var i = 0; i < 1; ++i) { await i }',
    '(async () => { for (void (i = 0); i < 1; ++i) { await i } })()' ],
  [ 'for (let i = 0; i < 1; ++i) { await i }',
    '(async () => { for (let i = 0; i < 1; ++i) { await i } })()' ],
  [ 'var {a} = {a:1}, [b] = [1], {c:{d}} = {c:{d: await 1}}',
    '(async () => { void ( ({a} = {a:1}), ([b] = [1]), ' +
        '({c:{d}} = {c:{d: await 1}})) })()' ],
  /* eslint-disable no-template-curly-in-string */
  [ 'console.log(`${(await { a: 1 }).a}`)',
    '(async () => { return (console.log(`${(await { a: 1 }).a}`)) })()' ],
  /* eslint-enable no-template-curly-in-string */
  [ 'await 0; function foo() {}',
    '(async () => { await 0; foo=function foo() {} })()' ],
  [ 'await 0; class Foo {}',
    '(async () => { await 0; Foo=class Foo {} })()' ],
  [ 'if (await true) { function foo() {} }',
    '(async () => { if (await true) { foo=function foo() {} } })()' ],
  [ 'if (await true) { class Foo{} }',
    '(async () => { if (await true) { class Foo{} } })()' ],
  [ 'if (await true) { var a = 1; }',
    '(async () => { if (await true) { void (a = 1); } })()' ],
  [ 'if (await true) { let a = 1; }',
    '(async () => { if (await true) { let a = 1; } })()' ],
  [ 'var a = await 1; let b = 2; const c = 3;',
    '(async () => { void (a = await 1); void (b = 2); void (c = 3); })()' ],
  [ 'let o = await 1, p',
    '(async () => { void ( (o = await 1), (p=undefined)) })()' ]
];

for (const [input, expected] of testCases) {
  assert.strictEqual(processTopLevelAwait(input), expected);
}
