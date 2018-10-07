'use strict';

const acorn = require('internal/deps/acorn/dist/acorn');
const { tokTypes: tt } = acorn;

// If the error is that we've unexpectedly ended the input,
// then let the user try to recover by adding more input.
// Note: `e` (the original exception) is not used by the current implementation,
// but may be needed in the future.
function isRecoverableError(e, code) {
  let recoverable = false;

  // Determine if the point of the any error raised is at the end of the input.
  // There are two cases to consider:
  //
  //   1.  Any error raised after we have encountered the 'eof' token.
  //       This prevents us from declaring partial tokens (like '2e') as
  //       recoverable.
  //
  //   2.  Three cases where tokens can legally span lines.  This is
  //       template, comment, and strings with a backslash at the end of
  //       the line, indicating a continuation.  Note that we need to look
  //       for the specific errors of 'unterminated' kind (not, for example,
  //       a syntax error in a ${} expression in a template), and the only
  //       way to do that currently is to look at the message.  Should Acorn
  //       change these messages in the future, this will lead to a test
  //       failure, indicating that this code needs to be updated.
  //
  acorn.plugins.replRecoverable = (parser) => {
    parser.extend('nextToken', (nextToken) => {
      return function() {
        Reflect.apply(nextToken, this, []);

        if (this.type === tt.eof) recoverable = true;
      };
    });

    parser.extend('raise', (raise) => {
      return function(pos, message) {
        switch (message) {
          case 'Unterminated template':
          case 'Unterminated comment':
            recoverable = true;
            break;

          case 'Unterminated string constant':
            const token = this.input.slice(this.lastTokStart, this.pos);
            // see https://www.ecma-international.org/ecma-262/#sec-line-terminators
            recoverable = /\\(?:\r\n?|\n|\u2028|\u2029)$/.test(token);
        }

        Reflect.apply(raise, this, [pos, message]);
      };
    });
  };

  // For similar reasons as `defaultEval`, wrap expressions starting with a
  // curly brace with parenthesis.  Note: only the open parenthesis is added
  // here as the point is to test for potentially valid but incomplete
  // expressions.
  if (/^\s*\{/.test(code) && isRecoverableError(e, `(${code}`)) return true;

  // Try to parse the code with acorn.  If the parse fails, ignore the acorn
  // error and return the recoverable status.
  try {
    acorn.parse(code, { plugins: { replRecoverable: true }, ecmaVersion: 10 });

    // Odd case: the underlying JS engine (V8, Chakra) rejected this input
    // but Acorn detected no issue.  Presume that additional text won't
    // address this issue.
    return false;
  } catch {
    return recoverable;
  }
}

module.exports = {
  isRecoverableError
};
