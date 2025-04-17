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
        const objWithGetters = {
          foo: { bar: { baz: {} }, get gBar() { return { baz: {} } } },
          get gFoo() { return { bar: { baz: {} } }; }
        };
        `, [
          ['objWithGetters.', ['objWithGetters.foo']],
          ['objWithGetters.f', ['objWithGetters.foo']],
          ['objWithGetters.foo', ['objWithGetters.foo']],
          ['objWithGetters.foo.', ['objWithGetters.foo.bar']],
          ['objWithGetters.foo.bar.b', ['objWithGetters.foo.bar.baz']],
          ['objWithGetters.gFo', ['objWithGetters.gFoo']],
          ['objWithGetters.foo.gB', ['objWithGetters.foo.gBar']],
        ]);
    });

    test('no completions are generated for properties that trigger getters', () => {
      runCompletionTests(
        `
        const objWithGetters = {
          foo: { bar: { baz: {} }, get gBar() { return { baz: {} } } },
          get gFoo() { return { bar: { baz: {} } }; }
        };
        `,
        [
          ['objWithGetters.gFoo.', []],
          ['objWithGetters.gFoo.b', []],
          ['objWithGetters.gFoo.bar.b', []],
          ['objWithGetters.foo.gBar.', []],
          ['objWithGetters.foo.gBar.b', []],
        ]);
    });
  });

  describe('completions on proxies', () => {
    test('no completions are generated for a proxy object', () => {
      runCompletionTests('const proxyObj = new Proxy({ foo: { bar: { baz: {} } } }, {});', [
        ['proxyObj.', []],
        ['proxyObj.f', []],
        ['proxyObj.foo', []],
        ['proxyObj.foo.', []],
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
        ]);
    });
  });
});
