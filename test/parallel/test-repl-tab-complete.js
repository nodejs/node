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
const ArrayStream = require('../common/arraystream');
const { describe, it } = require('node:test');
const assert = require('assert');

const repl = require('repl');

function getNoResultsFunction() {
  return common.mustSucceed((data) => {
    assert.deepStrictEqual(data[0], []);
  });
}

function prepareREPL() {
  const input = new ArrayStream();
  const replServer = repl.start({
    prompt: '',
    input,
    output: process.stdout,
    allowBlockingCompletions: true,
  });

  // Some errors are passed to the domain, but do not callback
  replServer._domain.on('error', assert.ifError);

  return { replServer, input };
}

describe('REPL tab completion (core functionality)', () => {
  it('does not break in an object literal', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var inner = {', 'one:1']);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.complete(
      'console.lo',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [['console.log'], 'console.lo']);
      })
    );

    replServer.close();
  });

  it('works with optional chaining', () => {
    const { replServer } = prepareREPL();

    replServer.complete(
      'console?.lo',
      common.mustCall((_error, data) => {
        assert.deepStrictEqual(data, [['console?.log'], 'console?.lo']);
      })
    );

    replServer.complete(
      'console?.zzz',
      common.mustCall((_error, data) => {
        assert.deepStrictEqual(data, [[], 'console?.zzz']);
      })
    );

    replServer.complete(
      'console?.',
      common.mustCall((_error, data) => {
        assert(data[0].includes('console?.log'));
        assert.strictEqual(data[1], 'console?.');
      })
    );

    replServer.close();
  });

  it('returns object completions', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var inner = {', 'one:1']);

    input.run(['};']);

    replServer.complete(
      'inner.o',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [['inner.one'], 'inner.o']);
      })
    );

    replServer.close();
  });

  it('does not break in a ternary operator with ()', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var inner = ( true ', '?', '{one: 1} : ']);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it('works on literals', () => {
    const { replServer } = prepareREPL();

    replServer.complete(
      '``.a',
      common.mustCall((err, data) => {
        assert.strictEqual(data[0].includes('``.at'), true);
      })
    );
    replServer.complete(
      "''.a",
      common.mustCall((err, data) => {
        assert.strictEqual(data[0].includes("''.at"), true);
      })
    );
    replServer.complete(
      '"".a',
      common.mustCall((err, data) => {
        assert.strictEqual(data[0].includes('"".at'), true);
      })
    );
    replServer.complete(
      '("").a',
      common.mustCall((err, data) => {
        assert.strictEqual(data[0].includes('("").at'), true);
      })
    );
    replServer.complete(
      '[].a',
      common.mustCall((err, data) => {
        assert.strictEqual(data[0].includes('[].at'), true);
      })
    );
    replServer.complete(
      '{}.a',
      common.mustCall((err, data) => {
        assert.deepStrictEqual(data[0], []);
      })
    );

    replServer.close();
  });

  it("does not return a function's local variable", () => {
    const { replServer, input } = prepareREPL();

    input.run(['var top = function() {', 'var inner = {one:1};', '}']);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it("does not return a function's local variable even when the function has parameters", () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var top = function(one, two) {',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it("does not return a function's local variable" +
    'even if the scope is nested inside an immediately executed function', () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var top = function() {',
      '(function test () {',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it("does not return a function's local variable" +
    'even if the scope is nested inside an immediately executed function' +
    '(the definition has the params and { on a separate line)', () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var top = function() {',
      'r = function test (',
      ' one, two) {',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it('currently does not work, but should not break (local inner)', () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var top = function() {',
      'r = function test ()',
      '{',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it('currently does not work, but should not break (local inner parens next line)', () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var top = function() {',
      'r = function test (',
      ')',
      '{',
      'var inner = {',
      ' one:1',
      '};',
    ]);

    replServer.complete('inner.o', getNoResultsFunction());

    replServer.close();
  });

  it('works on non-Objects', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var str = "test";']);

    replServer.complete(
      'str.len',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [['str.length'], 'str.len']);
      })
    );

    replServer.close();
  });

  it('should be case-insensitive if member part is lower-case', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var foo = { barBar: 1, BARbuz: 2, barBLA: 3 };']);

    replServer.complete(
      'foo.b',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [
          ['foo.BARbuz', 'foo.barBLA', 'foo.barBar'],
          'foo.b',
        ]);
      })
    );

    replServer.close();
  });

  it('should be case-insensitive if member part is upper-case', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var foo = { barBar: 1, BARbuz: 2, barBLA: 3 };']);

    replServer.complete(
      'foo.B',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [
          ['foo.BARbuz', 'foo.barBLA', 'foo.barBar'],
          'foo.B',
        ]);
      })
    );

    replServer.close();
  });

  it('should not break on spaces', () => {
    const { replServer } = prepareREPL();

    const spaceTimeout = setTimeout(function() {
      throw new Error('timeout');
    }, 1000);

    replServer.complete(
      ' ',
      common.mustSucceed((data) => {
        assert.strictEqual(data[1], '');
        assert.ok(data[0].includes('globalThis'));
        clearTimeout(spaceTimeout);
      })
    );

    replServer.close();
  });

  it(`should pick up the global "toString" object, and any other properties up the "global" object's prototype chain`, () => {
    const { replServer } = prepareREPL();

    replServer.complete(
      'toSt',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [['toString'], 'toSt']);
      })
    );

    replServer.close();
  });

  it('should make own properties shadow properties on the prototype', () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var x = Object.create(null);',
      'x.a = 1;',
      'x.b = 2;',
      'var y = Object.create(x);',
      'y.a = 3;',
      'y.c = 4;',
    ]);

    replServer.complete(
      'y.',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [['y.b', '', 'y.a', 'y.c'], 'y.']);
      })
    );

    replServer.close();
  });

  it('works on context properties', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var custom = "test";']);

    replServer.complete(
      'cus',
      common.mustCall(function(_error, data) {
        assert.deepStrictEqual(data, [['CustomEvent', 'custom'], 'cus']);
      })
    );

    replServer.close();
  });

  it("doesn't crash REPL with half-baked proxy objects", () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'var proxy = new Proxy({}, {ownKeys: () => { throw new Error(); }});',
    ]);

    replServer.complete(
      'proxy.',
      common.mustCall(function(error, data) {
        assert.strictEqual(error, null);
        assert(Array.isArray(data));
      })
    );

    replServer.close();
  });

  it('does not include integer members of an Array', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var ary = [1,2,3];']);

    replServer.complete(
      'ary.',
      common.mustCall(function(_error, data) {
        assert.strictEqual(data[0].includes('ary.0'), false);
        assert.strictEqual(data[0].includes('ary.1'), false);
        assert.strictEqual(data[0].includes('ary.2'), false);
      })
    );

    replServer.close();
  });

  it('does not include integer keys in an object', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var obj = {1:"a","1a":"b",a:"b"};']);

    replServer.complete(
      'obj.',
      common.mustCall(function(_error, data) {
        assert.strictEqual(data[0].includes('obj.1'), false);
        assert.strictEqual(data[0].includes('obj.1a'), false);
        assert(data[0].includes('obj.a'));
      })
    );

    replServer.close();
  });

  it('does not try to complete results of non-simple expressions', () => {
    const { replServer, input } = prepareREPL();

    input.run(['function a() {}']);

    replServer.complete('a().b.', getNoResultsFunction());

    replServer.close();
  });

  it('works when prefixed with spaces', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var obj = {1:"a","1a":"b",a:"b"};']);

    replServer.complete(
      ' obj.',
      common.mustCall((_error, data) => {
        assert.strictEqual(data[0].includes('obj.1'), false);
        assert.strictEqual(data[0].includes('obj.1a'), false);
        assert(data[0].includes('obj.a'));
      })
    );

    replServer.close();
  });

  it('works inside assignments', () => {
    const { replServer } = prepareREPL();

    replServer.complete(
      'var log = console.lo',
      common.mustCall((_error, data) => {
        assert.deepStrictEqual(data, [['console.log'], 'console.lo']);
      })
    );

    replServer.close();
  });

  it('works for defined commands', () => {
    const { replServer, input } = prepareREPL();

    replServer.complete(
      '.b',
      common.mustCall((error, data) => {
        assert.deepStrictEqual(data, [['break'], 'b']);
      })
    );

    input.run(['var obj = {"hello, world!": "some string", "key": 123}']);

    replServer.complete(
      'obj.',
      common.mustCall((error, data) => {
        assert.strictEqual(data[0].includes('obj.hello, world!'), false);
        assert(data[0].includes('obj.key'));
      })
    );

    replServer.close();
  });

  it('does not include __defineSetter__ and friends', () => {
    const { replServer, input } = prepareREPL();

    input.run(['var obj = {};']);

    replServer.complete(
      'obj.',
      common.mustCall(function(error, data) {
        assert.strictEqual(data[0].includes('obj.__defineGetter__'), false);
        assert.strictEqual(data[0].includes('obj.__defineSetter__'), false);
        assert.strictEqual(data[0].includes('obj.__lookupGetter__'), false);
        assert.strictEqual(data[0].includes('obj.__lookupSetter__'), false);
        assert.strictEqual(data[0].includes('obj.__proto__'), true);
      })
    );

    replServer.close();
  });

  it('works with builtin values', () => {
    const { replServer } = prepareREPL();

    replServer.complete(
      'I',
      common.mustCall((error, data) => {
        assert.deepStrictEqual(data, [
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
      })
    );

    replServer.close();
  });

  it('works with lexically scoped variables', () => {
    const { replServer, input } = prepareREPL();

    input.run([
      'let lexicalLet = true;',
      'const lexicalConst = true;',
      'class lexicalKlass {}',
    ]);

    ['Let', 'Const', 'Klass'].forEach((type) => {
      const query = `lexical${type[0]}`;
      const hasInspector = process.features.inspector;
      const expected = hasInspector ?
        [[`lexical${type}`], query] :
        [[], `lexical${type[0]}`];
      replServer.complete(
        query,
        common.mustCall((error, data) => {
          assert.deepStrictEqual(data, expected);
        })
      );
    });

    replServer.close();
  });
});
