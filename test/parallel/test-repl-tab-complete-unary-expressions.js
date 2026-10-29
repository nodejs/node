'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');
const { describe, it } = require('node:test');

// This test verifies that tab completion works correctly with unary expressions
// like delete, typeof, void, etc. This is a regression test for the issue where
// typing "delete globalThis._" and then backspacing and typing "globalThis"
// would cause "globalThis is not defined" error.

describe('REPL tab completion with unary expressions', () => {
  it('should handle delete operator correctly', (t, done) => {
    const { replServer } = startNewREPLServer({ terminal: false });

    // Test delete with member expression
    replServer.complete(
      'delete globalThis._',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'globalThis._');

        // Test delete with identifier
        replServer.complete(
          'delete globalThis',
          common.mustSucceed((completions) => {
            assert.strictEqual(completions[1], 'globalThis');
            replServer.close();
            done();
          })
        );
      })
    );
  });

  it('should handle typeof operator correctly', (t, done) => {
    const { replServer } = startNewREPLServer({ terminal: false });

    replServer.complete(
      'typeof globalThis',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'globalThis');
        replServer.close();
        done();
      })
    );
  });

  it('should handle void operator correctly', (t, done) => {
    const { replServer } = startNewREPLServer({ terminal: false });

    replServer.complete(
      'void globalThis',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'globalThis');
        replServer.close();
        done();
      })
    );
  });

  it('should handle other unary operators correctly', (t, done) => {
    const { replServer } = startNewREPLServer({ terminal: false });

    const unaryOperators = [
      '!globalThis',
      '+globalThis',
      '-globalThis',
      '~globalThis',
    ];

    let testIndex = 0;

    function testNext() {
      if (testIndex >= unaryOperators.length) {
        replServer.close();
        done();
        return;
      }

      const testCase = unaryOperators[testIndex++];
      replServer.complete(
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
    const { replServer } = startNewREPLServer({ terminal: false });

    // First trigger completion with delete
    replServer.complete(
      'delete globalThis._',
      common.mustSucceed(() => {
        // Then evaluate globalThis
        replServer.eval(
          'globalThis',
          replServer.context,
          'test.js',
          common.mustSucceed((result) => {
            assert.strictEqual(typeof result, 'object');
            assert.ok(result !== null);
            replServer.close();
            done();
          })
        );
      })
    );
  });
});
