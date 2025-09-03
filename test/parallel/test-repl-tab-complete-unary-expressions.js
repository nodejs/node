'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const { describe, it } = require('node:test');

// This test verifies that tab completion works correctly with unary expressions
// like delete, typeof, void, etc. This is a regression test for the issue where
// typing "delete globalThis._" and then backspacing and typing "globalThis"
// would cause "globalThis is not defined" error.

describe('REPL tab completion with unary expressions', () => {
  it('should handle delete operator correctly', (t, done) => {
    const r = repl.start({
      prompt: '',
      input: process.stdin,
      output: process.stdout,
      terminal: false,
    });

    // Test delete with member expression
    r.complete(
      'delete globalThis._',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'globalThis._');

        // Test delete with identifier
        r.complete(
          'delete globalThis',
          common.mustSucceed((completions) => {
            assert.strictEqual(completions[1], 'globalThis');
            r.close();
            done();
          })
        );
      })
    );
  });

  it('should handle typeof operator correctly', (t, done) => {
    const r = repl.start({
      prompt: '',
      input: process.stdin,
      output: process.stdout,
      terminal: false,
    });

    r.complete(
      'typeof globalThis',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'globalThis');
        r.close();
        done();
      })
    );
  });

  it('should handle void operator correctly', (t, done) => {
    const r = repl.start({
      prompt: '',
      input: process.stdin,
      output: process.stdout,
      terminal: false,
    });

    r.complete(
      'void globalThis',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'globalThis');
        r.close();
        done();
      })
    );
  });

  it('should handle other unary operators correctly', (t, done) => {
    const r = repl.start({
      prompt: '',
      input: process.stdin,
      output: process.stdout,
      terminal: false,
    });

    const unaryOperators = [
      '!globalThis',
      '+globalThis',
      '-globalThis',
      '~globalThis',
    ];

    let testIndex = 0;

    function testNext() {
      if (testIndex >= unaryOperators.length) {
        r.close();
        done();
        return;
      }

      const testCase = unaryOperators[testIndex++];
      r.complete(
        testCase,
        common.mustSucceed((completions) => {
          assert.strictEqual(completions[1], 'globalThis');
          testNext();
        })
      );
    }

    testNext();
  });

  it('should still evaluate globalThis correctly after unary expression completion', (t, done) => {
    const r = repl.start({
      prompt: '',
      input: process.stdin,
      output: process.stdout,
      terminal: false,
    });

    // First trigger completion with delete
    r.complete(
      'delete globalThis._',
      common.mustSucceed(() => {
        // Then evaluate globalThis
        r.eval(
          'globalThis',
          r.context,
          'test.js',
          common.mustSucceed((result) => {
            assert.strictEqual(typeof result, 'object');
            assert.ok(result !== null);
            r.close();
            done();
          })
        );
      })
    );
  });
});
