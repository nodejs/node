'use strict';

const common = require('../common');
const assert = require('node:assert');
const { describe, test } = require('node:test');

const ArrayStream = require('../common/arraystream');

const repl = require('node:repl');

function runCompletionTests(replInit, tests) {
  const stream = new ArrayStream();
  const testRepl = repl.start({ stream });

  // Some errors are passed to the domain
  testRepl._domain.on('error', assert.ifError);

  testRepl.write(replInit);
  testRepl.write('\n');

  tests.forEach(([query, expectedCompletions]) => {
    testRepl.complete(query, common.mustCall((error, data) => {
      const actualCompletions = data[0];
      if (expectedCompletions.length === 0) {
        assert.deepStrictEqual(actualCompletions, []);
      } else {
        expectedCompletions.forEach((expectedCompletion) =>
          assert(actualCompletions.includes(expectedCompletion), `completion '${expectedCompletion}' not found`)
        );
      }
    }));
  });
}

describe('REPL completion in relation of getters', () => {
  describe('standard behavior without proxies/getters', () => {
    test('completion of nested properties of an undeclared objects', () => {
      runCompletionTests('', [
        ['nonExisting.', []],
        ['nonExisting.f', []],
        ['nonExisting.foo', []],
        ['nonExisting.foo.', []],
        ['nonExisting.foo.bar.b', []],
      ]);
    });

    test('completion of nested properties on plain objects', () => {
      runCompletionTests('const plainObj = { foo: { bar: { baz: {} } } };', [
        ['plainObj.', ['plainObj.foo']],
        ['plainObj.f', ['plainObj.foo']],
        ['plainObj.foo', ['plainObj.foo']],
        ['plainObj.foo.', ['plainObj.foo.bar']],
        ['plainObj.foo.bar.b', ['plainObj.foo.bar.baz']],
        ['plainObj.fooBar.', []],
        ['plainObj.fooBar.baz', []],
      ]);
    });
  });

  describe('completions on an object with getters', () => {
    test(`completions are generated for properties that don't trigger getters`, () => {
      runCompletionTests(
        `
        function getFooKey() {
          return "foo";
        }

        const fooKey = "foo";

        const keys = {
          "foo key": "foo",
        };

        const objWithGetters = {
          foo: { bar: { baz: { buz: {} } }, get gBar() { return { baz: {} } } },
          get gFoo() { return { bar: { baz: {} } }; }
        };
        `, [
          ['objWithGetters.', ['objWithGetters.foo']],
          ['objWithGetters.f', ['objWithGetters.foo']],
          ['objWithGetters.foo', ['objWithGetters.foo']],
          ['objWithGetters["foo"].b', ['objWithGetters["foo"].bar']],
          ['objWithGetters.foo.', ['objWithGetters.foo.bar']],
          ['objWithGetters.foo.bar.b', ['objWithGetters.foo.bar.baz']],
          ['objWithGetters.gFo', ['objWithGetters.gFoo']],
          ['objWithGetters.foo.gB', ['objWithGetters.foo.gBar']],
          ["objWithGetters.foo['bar'].b", ["objWithGetters.foo['bar'].baz"]],
          ["objWithGetters['foo']['bar'].b", ["objWithGetters['foo']['bar'].baz"]],
          ["objWithGetters['foo']['bar']['baz'].b", ["objWithGetters['foo']['bar']['baz'].buz"]],
          ["objWithGetters[keys['foo key']].b", ["objWithGetters[keys['foo key']].bar"]],
          ['objWithGetters[fooKey].b', ['objWithGetters[fooKey].bar']],
          ["objWithGetters['f' + 'oo'].b", ["objWithGetters['f' + 'oo'].bar"]],
        ]);
    });

    test('no completions are generated for properties that trigger getters', () => {
      runCompletionTests(
        `
        function getGFooKey() {
          return "g" + "Foo";
        }

        const gFooKey = "gFoo";

        const keys = {
          "g-foo key": "gFoo",
        };

        const objWithGetters = {
          foo: { bar: { baz: {} }, get gBar() { return { baz: {}, get gBuz() { return 5; } } } },
          get gFoo() { return { bar: { baz: {} } }; }
        };
        `,
        [
          ['objWithGetters.gFoo.', []],
          ['objWithGetters.gFoo.b', []],
          ['objWithGetters["gFoo"].b', []],
          ['objWithGetters.gFoo.bar.b', []],
          ['objWithGetters.foo.gBar.', []],
          ['objWithGetters.foo.gBar.b', []],
          ["objWithGetters.foo['gBar'].b", []],
          ["objWithGetters['foo']['gBar'].b", []],
          ["objWithGetters['foo']['gBar']['gBuz'].", []],
          ["objWithGetters[keys['g-foo key']].b", []],
          ['objWithGetters[gFooKey].b', []],
          ["objWithGetters['g' + 'Foo'].b", []],
          ['objWithGetters[getGFooKey()].b', []],
        ]);
    });
  });

  describe('completions on proxies', () => {
    test('no completions are generated for a proxy object', () => {
      runCompletionTests(
        `
        function getFooKey() {
          return "foo";
        }

        const fooKey = "foo";

        const keys = {
          "foo key": "foo",
        };

        const proxyObj = new Proxy({ foo: { bar: { baz: {} } } }, {});
        `, [
          ['proxyObj.', []],
          ['proxyObj.f', []],
          ['proxyObj.foo', []],
          ['proxyObj.foo.', []],
          ['proxyObj.["foo"].', []],
          ['proxyObj.["f" + "oo"].', []],
          ['proxyObj.[fooKey].', []],
          ['proxyObj.[getFooKey()].', []],
          ['proxyObj.[keys["foo key"]].', []],
          ['proxyObj.foo.bar.b', []],
        ]);
    });

    test('no completions are generated for a proxy present in a standard object', () => {
      runCompletionTests(
        'const objWithProxy = { foo: { bar: new Proxy({ baz: {} }, {}) } };', [
          ['objWithProxy.', ['objWithProxy.foo']],
          ['objWithProxy.foo', ['objWithProxy.foo']],
          ['objWithProxy.foo.', ['objWithProxy.foo.bar']],
          ['objWithProxy.foo.b', ['objWithProxy.foo.bar']],
          ['objWithProxy.foo.bar.', []],
          ['objWithProxy.foo["b" + "ar"].', []],
        ]);
    });
  });
});
