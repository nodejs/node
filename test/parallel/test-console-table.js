'use strict';

require('../common');

const assert = require('assert');
const { Console } = require('console');

const queue = [];

const console = new Console({ write: (x) => {
  queue.push(x);
}, removeListener: () => {} }, process.stderr, false);

function test(data, only, expected) {
  if (arguments.length === 2) {
    expected = only;
    only = undefined;
  }
  console.table(data, only);
  assert.deepStrictEqual(
    queue.shift().split('\n'),
    expected.trimLeft().split('\n')
  );
}

assert.throws(() => console.table([], false), {
  code: 'ERR_INVALID_ARG_TYPE',
});

test(null, 'null\n');
test(undefined, 'undefined\n');
test(false, 'false\n');
test('hi', 'hi\n');
test(Symbol(), 'Symbol()\n');
test(function() {}, '[Function (anonymous)]\n');

test([1, 2, 3], `
┌─────────┬────────┐
│ (index) │ Values │
├─────────┼────────┤
│ 0       │ 1      │
│ 1       │ 2      │
│ 2       │ 3      │
└─────────┴────────┘
`);

test([Symbol(), 5, [10]], `
┌─────────┬────┬──────────┐
│ (index) │ 0  │ Values   │
├─────────┼────┼──────────┤
│ 0       │    │ Symbol() │
│ 1       │    │ 5        │
│ 2       │ 10 │          │
└─────────┴────┴──────────┘
`);

test([null, 5], `
┌─────────┬────────┐
│ (index) │ Values │
├─────────┼────────┤
│ 0       │ null   │
│ 1       │ 5      │
└─────────┴────────┘
`);

test([undefined, 5], `
┌─────────┬───────────┐
│ (index) │ Values    │
├─────────┼───────────┤
│ 0       │ undefined │
│ 1       │ 5         │
└─────────┴───────────┘
`);

test({ a: 1, b: Symbol(), c: [10] }, `
┌─────────┬────┬──────────┐
│ (index) │ 0  │ Values   │
├─────────┼────┼──────────┤
│ a       │    │ 1        │
│ b       │    │ Symbol() │
│ c       │ 10 │          │
└─────────┴────┴──────────┘
`);

test(new Map([ ['a', 1], [Symbol(), [2]] ]), `
┌───────────────────┬──────────┬────────┐
│ (iteration index) │ Key      │ Values │
├───────────────────┼──────────┼────────┤
│ 0                 │ 'a'      │ 1      │
│ 1                 │ Symbol() │ [ 2 ]  │
└───────────────────┴──────────┴────────┘
`);

test(new Set([1, 2, Symbol()]), `
┌───────────────────┬──────────┐
│ (iteration index) │ Values   │
├───────────────────┼──────────┤
│ 0                 │ 1        │
│ 1                 │ 2        │
│ 2                 │ Symbol() │
└───────────────────┴──────────┘
`);

test({ a: 1, b: 2 }, ['a'], `
┌─────────┬───┐
│ (index) │ a │
├─────────┼───┤
│ a       │   │
│ b       │   │
└─────────┴───┘
`);

test([{ a: 1, b: 2 }, { a: 3, c: 4 }], ['a'], `
┌─────────┬───┐
│ (index) │ a │
├─────────┼───┤
│ 0       │ 1 │
│ 1       │ 3 │
└─────────┴───┘
`);

test(new Map([[1, 1], [2, 2], [3, 3]]).entries(), `
┌───────────────────┬─────┬────────┐
│ (iteration index) │ Key │ Values │
├───────────────────┼─────┼────────┤
│ 0                 │ 1   │ 1      │
│ 1                 │ 2   │ 2      │
│ 2                 │ 3   │ 3      │
└───────────────────┴─────┴────────┘
`);

test(new Map([[1, 1], [2, 2], [3, 3]]).values(), `
┌───────────────────┬────────┐
│ (iteration index) │ Values │
├───────────────────┼────────┤
│ 0                 │ 1      │
│ 1                 │ 2      │
│ 2                 │ 3      │
└───────────────────┴────────┘
`);

test(new Map([[1, 1], [2, 2], [3, 3]]).keys(), `
┌───────────────────┬────────┐
│ (iteration index) │ Values │
├───────────────────┼────────┤
│ 0                 │ 1      │
│ 1                 │ 2      │
│ 2                 │ 3      │
└───────────────────┴────────┘
`);

test(new Set([1, 2, 3]).values(), `
┌───────────────────┬────────┐
│ (iteration index) │ Values │
├───────────────────┼────────┤
│ 0                 │ 1      │
│ 1                 │ 2      │
│ 2                 │ 3      │
└───────────────────┴────────┘
`);


test({ a: { a: 1, b: 2, c: 3 } }, `
┌─────────┬───┬───┬───┐
│ (index) │ a │ b │ c │
├─────────┼───┼───┼───┤
│ a       │ 1 │ 2 │ 3 │
└─────────┴───┴───┴───┘
`);

test({ a: { a: { a: 1, b: 2, c: 3 } } }, `
┌─────────┬──────────┐
│ (index) │ a        │
├─────────┼──────────┤
│ a       │ [Object] │
└─────────┴──────────┘
`);

test({ a: [1, 2] }, `
┌─────────┬───┬───┐
│ (index) │ 0 │ 1 │
├─────────┼───┼───┤
│ a       │ 1 │ 2 │
└─────────┴───┴───┘
`);

test({ a: [1, 2, 3, 4, 5], b: 5, c: { e: 5 } }, `
┌─────────┬───┬───┬───┬───┬───┬───┬────────┐
│ (index) │ 0 │ 1 │ 2 │ 3 │ 4 │ e │ Values │
├─────────┼───┼───┼───┼───┼───┼───┼────────┤
│ a       │ 1 │ 2 │ 3 │ 4 │ 5 │   │        │
│ b       │   │   │   │   │   │   │ 5      │
│ c       │   │   │   │   │   │ 5 │        │
└─────────┴───┴───┴───┴───┴───┴───┴────────┘
`);

test(new Uint8Array([1, 2, 3]), `
┌─────────┬────────┐
│ (index) │ Values │
├─────────┼────────┤
│ 0       │ 1      │
│ 1       │ 2      │
│ 2       │ 3      │
└─────────┴────────┘
`);

test(Buffer.from([1, 2, 3]), `
┌─────────┬────────┐
│ (index) │ Values │
├─────────┼────────┤
│ 0       │ 1      │
│ 1       │ 2      │
│ 2       │ 3      │
└─────────┴────────┘
`);

test({ a: undefined }, ['x'], `
┌─────────┬───┐
│ (index) │ x │
├─────────┼───┤
│ a       │   │
└─────────┴───┘
`);

test([], `
┌─────────┐
│ (index) │
├─────────┤
└─────────┘
`);

test(new Map(), `
┌───────────────────┬─────┬────────┐
│ (iteration index) │ Key │ Values │
├───────────────────┼─────┼────────┤
└───────────────────┴─────┴────────┘
`);

test([{ a: 1, b: 'Y' }, { a: 'Z', b: 2 }], `
┌─────────┬─────┬─────┐
│ (index) │ a   │ b   │
├─────────┼─────┼─────┤
│ 0       │ 1   │ 'Y' │
│ 1       │ 'Z' │ 2   │
└─────────┴─────┴─────┘
`);

{
  const line = '─'.repeat(79);
  const header = `name${' '.repeat(77)}`;
  const name = 'very long long long long long long long long long long long ' +
               'long long long long';
  test([{ name }], `
┌─────────┬──${line}──┐
│ (index) │ ${header} │
├─────────┼──${line}──┤
│ 0       │ '${name}' │
└─────────┴──${line}──┘
`);
}

test({ foo: '￥', bar: '¥' }, `
┌─────────┬────────┐
│ (index) │ Values │
├─────────┼────────┤
│ foo     │ '￥'   │
│ bar     │ '¥'    │
└─────────┴────────┘
`);

test({ foo: '你好', bar: 'hello' }, `
┌─────────┬─────────┐
│ (index) │ Values  │
├─────────┼─────────┤
│ foo     │ '你好'  │
│ bar     │ 'hello' │
└─────────┴─────────┘
`);

// Regression test for prototype pollution via console.table. Earlier versions
// of Node.js created an object with a non-null prototype within console.table
// and then wrote to object[column][index], which lead to an error as well as
// modifications to Object.prototype.
test([{ foo: 10 }, { foo: 20 }], ['__proto__'], `
┌─────────┬───────────┐
│ (index) │ __proto__ │
├─────────┼───────────┤
│ 0       │           │
│ 1       │           │
└─────────┴───────────┘
`);
assert.strictEqual('0' in Object.prototype, false);
assert.strictEqual('1' in Object.prototype, false);
