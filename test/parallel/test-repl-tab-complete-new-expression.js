'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');
const { describe, it } = require('node:test');

// This test verifies that tab completion works correctly with `new` operator
// for a class. Property access has higher precedence than `new` so the properties
// should be displayed as autocompletion result.

describe('REPL tab completion with new expressions', () => {
  it('should output completion of class properties', () => {
    const { replServer, input } = startNewREPLServer({ terminal: false });

    input.run([
      `
                class X { x = 1 };
                X.Y = class Y { y = 2 };
            `,
    ]);

    // Handle completion for property of root class.
    replServer.complete(
      'new X.',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'X.');
      })
    );

    // Handle completion for property with another class as value.
    replServer.complete(
      'new X.Y.',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'X.Y.');
      })
    );

    replServer.close();
  });
});
