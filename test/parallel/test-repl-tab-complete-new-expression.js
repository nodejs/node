'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');
const { describe, it } = require('node:test');

// This test verifies that tab completion works correctly with `new` operator
// for a class. Property access has higher precedence than `new` so the properties
// should be displayed as autocompletion result.

describe('REPL tab completion with new expressions', () => {
  it('should output completion of class properties', (_, done) => {
    const { replServer, input } = startNewREPLServer({ terminal: false });

    input.run([
      `
                class X { x = 1 };
                X.Y = class Y { y = 2 };
            `,
    ]);

    replServer.complete(
      'new X.',
      common.mustSucceed((completions) => {
        assert.strictEqual(completions[1], 'X.');
        replServer.close();
        done();
      })
    );
  });
});
