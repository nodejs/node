// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');
const { startNewREPLServer, complete } = require('../common/repl');
const { describe, it } = require('node:test');
const assert = require('assert');

async function expectNoResults(replServer, query) {
  const data = await complete(replServer, query);
  assert.deepStrictEqual(data[0], []);
}

describe('REPL tab completion (core functionality)', () => {
  it('does not break with variable declarations without an initialization', async () => {
    const { replServer } = startNewREPLServer();
    await expectNoResults(replServer, 'let a');
    replServer.close();
  });

  it('does not break in an object literal', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var inner = {', 'one:1']);

    await expectNoResults(replServer, 'inner.o');

    assert.deepStrictEqual(
      await complete(replServer, 'console.lo'),
      [['console.log'], 'console.lo']);

    replServer.close();
  });

  it('works with optional chaining', async () => {
    const { replServer } = startNewREPLServer();

    assert.deepStrictEqual(
      await complete(replServer, 'console?.lo'),
      [['console?.log'], 'console?.lo']);

    assert.deepStrictEqual(
      await complete(replServer, 'console?.zzz'),
      [[], 'console?.zzz']);

    const data = await complete(replServer, 'console?.');
    assert(data[0].includes('console?.log'));
    assert.strictEqual(data[1], 'console?.');

    replServer.close();
  });

  it('returns object completions', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var inner = {', 'one:1']);

    await run(['};']);

    assert.deepStrictEqual(
      await complete(replServer, 'inner.o'),
      [['inner.one'], 'inner.o']);

    replServer.close();
  });

  it('does not break in a ternary operator with ()', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var inner = ( true ', '?', '{one: 1} : ']);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it('works on literals', async () => {
    const { replServer } = startNewREPLServer();

    assert.strictEqual((await complete(replServer, '``.a'))[0].includes('``.at'), true);
    assert.strictEqual((await complete(replServer, "''.a"))[0].includes("''.at"), true);
    assert.strictEqual((await complete(replServer, '"".a'))[0].includes('"".at'), true);
    assert.strictEqual((await complete(replServer, '("").a'))[0].includes('("").at'), true);
    assert.strictEqual((await complete(replServer, '[].a'))[0].includes('[].at'), true);
    assert.deepStrictEqual((await complete(replServer, '{}.a'))[0], []);

    replServer.close();
  });

  it("does not return a function's local variable", async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var top = function() {', 'var inner = {one:1};', '}']);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it("does not return a function's local variable even when the function has parameters", async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var top = function(one, two) {',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it("does not return a function's local variable" +
    'even if the scope is nested inside an immediately executed function', async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var top = function() {',
      '(function test () {',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it("does not return a function's local variable" +
    'even if the scope is nested inside an immediately executed function' +
    '(the definition has the params and { on a separate line)', async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var top = function() {',
      'r = function test (',
      ' one, two) {',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it('currently does not work, but should not break (local inner)', async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var top = function() {',
      'r = function test ()',
      '{',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it('currently does not work, but should not break (local inner parens next line)', async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var top = function() {',
      'r = function test (',
      ')',
      '{',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    await expectNoResults(replServer, 'inner.o');

    replServer.close();
  });

  it('works on non-Objects', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var str = "test";']);

    assert.deepStrictEqual(
      await complete(replServer, 'str.len'),
      [['str.length'], 'str.len']);

    replServer.close();
  });

  it('should be case-insensitive if member part is lower-case', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var foo = { barBar: 1, BARbuz: 2, barBLA: 3 };']);

    assert.deepStrictEqual(
      await complete(replServer, 'foo.b'),
      [['foo.BARbuz', 'foo.barBLA', 'foo.barBar'], 'foo.b']);

    replServer.close();
  });

  it('should be case-insensitive if member part is upper-case', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var foo = { barBar: 1, BARbuz: 2, barBLA: 3 };']);

    assert.deepStrictEqual(
      await complete(replServer, 'foo.B'),
      [['foo.BARbuz', 'foo.barBLA', 'foo.barBar'], 'foo.B']);

    replServer.close();
  });

  it('should not break on spaces', async () => {
    const { replServer } = startNewREPLServer();

    const data = await complete(replServer, ' ');
    assert.strictEqual(data[1], '');
    assert.ok(data[0].includes('globalThis'));

    replServer.close();
  });

  it(`should pick up the global "toString" object, and any other properties up the "global" object's prototype chain`, async () => {
    const { replServer } = startNewREPLServer();

    assert.deepStrictEqual(
      await complete(replServer, 'toSt'),
      [['toString'], 'toSt']);

    replServer.close();
  });

  it('should make own properties shadow properties on the prototype', async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var x = Object.create(null);',
      'x.a = 1;',
      'x.b = 2;',
      'var y = Object.create(x);',
      'y.a = 3;',
      'y.c = 4;',
    ]);

    assert.deepStrictEqual(
      await complete(replServer, 'y.'),
      [['y.b', '', 'y.a', 'y.c'], 'y.']);

    replServer.close();
  });

  it('works on context properties', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var custom = "test";']);

    assert.deepStrictEqual(
      await complete(replServer, 'cus'),
      [['CustomEvent', 'custom'], 'cus']);

    replServer.close();
  });

  it("doesn't crash REPL with half-baked proxy objects", async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'var proxy = new Proxy({}, {ownKeys: () => { throw new Error(); }});',
    ]);

    const data = await complete(replServer, 'proxy.');
    assert(Array.isArray(data));

    replServer.close();
  });

  it('does not include integer members of an Array', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var ary = [1,2,3];']);

    const data = await complete(replServer, 'ary.');
    assert.strictEqual(data[0].includes('ary.0'), false);
    assert.strictEqual(data[0].includes('ary.1'), false);
    assert.strictEqual(data[0].includes('ary.2'), false);

    replServer.close();
  });

  it('does not include integer keys in an object', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var obj = {1:"a","1a":"b",a:"b"};']);

    const data = await complete(replServer, 'obj.');
    assert.strictEqual(data[0].includes('obj.1'), false);
    assert.strictEqual(data[0].includes('obj.1a'), false);
    assert(data[0].includes('obj.a'));

    replServer.close();
  });

  it('does not try to complete results of non-simple expressions', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['function a() {}']);

    await expectNoResults(replServer, 'a().b.');

    replServer.close();
  });

  it('works when prefixed with spaces', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var obj = {1:"a","1a":"b",a:"b"};']);

    const data = await complete(replServer, ' obj.');
    assert.strictEqual(data[0].includes('obj.1'), false);
    assert.strictEqual(data[0].includes('obj.1a'), false);
    assert(data[0].includes('obj.a'));

    replServer.close();
  });

  it('works inside assignments', async () => {
    const { replServer } = startNewREPLServer();

    assert.deepStrictEqual(
      await complete(replServer, 'var log = console.lo'),
      [['console.log'], 'console.lo']);

    replServer.close();
  });

  it('does not complete identifiers inside an unterminated string literal', async () => {
    // The completion point sits inside the text of an open string, where there
    // is nothing to complete. Regression test: previously the partial path was
    // mistaken for a bare identifier and completed against the global scope.
    const { replServer } = startNewREPLServer();

    await expectNoResults(replServer, 'const x = "./n');
    await expectNoResults(replServer, "const x = './n");
    await expectNoResults(replServer, 'const x = `./n');

    replServer.close();
  });

  it('does not complete inside fs path strings when blocking is disabled', async () => {
    // With `allowBlockingCompletions` off, `fs.<fn>("…` is just an open string;
    // it must not fall back to completing the partial path as a global.
    const { replServer } = startNewREPLServer({ allowBlockingCompletions: false });

    await expectNoResults(replServer, 'fs.readFileSync("./n');
    await expectNoResults(replServer, 'fs.promises.readFile("./n');
    await expectNoResults(replServer, 'fs.readFileSync(`./n');

    replServer.close();
  });

  it('works for defined commands', async () => {
    const { replServer, run } = startNewREPLServer();

    assert.deepStrictEqual(
      await complete(replServer, '.b'),
      [['break'], 'b']);

    await run(['var obj = {"hello, world!": "some string", "key": 123}']);

    const data = await complete(replServer, 'obj.');
    assert.strictEqual(data[0].includes('obj.hello, world!'), false);
    assert(data[0].includes('obj.key'));

    replServer.close();
  });

  it('does not include __defineSetter__ and friends', async () => {
    const { replServer, run } = startNewREPLServer();

    await run(['var obj = {};']);

    const data = await complete(replServer, 'obj.');
    assert.strictEqual(data[0].includes('obj.__defineGetter__'), false);
    assert.strictEqual(data[0].includes('obj.__defineSetter__'), false);
    assert.strictEqual(data[0].includes('obj.__lookupGetter__'), false);
    assert.strictEqual(data[0].includes('obj.__lookupSetter__'), false);
    assert.strictEqual(data[0].includes('obj.__proto__'), true);

    replServer.close();
  });

  it('works with builtin values', async () => {
    const { replServer } = startNewREPLServer();

    assert.deepStrictEqual(
      await complete(replServer, 'I'),
      [
        [
          'if',
          'import',
          'in',
          'instanceof',
          '',
          'Infinity',
          'Int16Array',
          'Int32Array',
          'Int8Array',
          ...(common.hasIntl ? ['Intl'] : []),
          'Iterator',
          'inspector',
          'isFinite',
          'isNaN',
          '',
          'isPrototypeOf',
        ],
        'I',
      ]);

    replServer.close();
  });

  it('works with lexically scoped variables', async () => {
    const { replServer, run } = startNewREPLServer();

    await run([
      'let lexicalLet = true;',
      'const lexicalConst = true;',
      'class lexicalKlass {}',
    ]);

    for (const type of ['Let', 'Const', 'Klass']) {
      const query = `lexical${type[0]}`;
      const expected = [[`lexical${type}`], query];
      assert.deepStrictEqual(await complete(replServer, query), expected);
    }

    replServer.close();
  });
});
