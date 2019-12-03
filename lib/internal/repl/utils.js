'use strict';

const acorn = require('internal/deps/acorn/acorn/dist/acorn');
const privateMethods =
  require('internal/deps/acorn-plugins/acorn-private-methods/index');
const classFields =
  require('internal/deps/acorn-plugins/acorn-class-fields/index');
const numericSeparator =
  require('internal/deps/acorn-plugins/acorn-numeric-separator/index');
const staticClassFeatures =
  require('internal/deps/acorn-plugins/acorn-static-class-features/index');
const { tokTypes: tt, Parser: AcornParser } = acorn;
const util = require('util');
const debug = require('internal/util/debuglog').debuglog('repl');

// If the error is that we've unexpectedly ended the input,
// then let the user try to recover by adding more input.
// Note: `e` (the original exception) is not used by the current implementation,
// but may be needed in the future.
function isRecoverableError(e, code) {
  // For similar reasons as `defaultEval`, wrap expressions starting with a
  // curly brace with parenthesis.  Note: only the open parenthesis is added
  // here as the point is to test for potentially valid but incomplete
  // expressions.
  if (/^\s*\{/.test(code) && isRecoverableError(e, `(${code}`)) return true;

  let recoverable = false;

  // Determine if the point of any error raised is at the end of the input.
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
  const RecoverableParser = AcornParser
    .extend(
      privateMethods,
      classFields,
      numericSeparator,
      staticClassFeatures,
      (Parser) => {
        return class extends Parser {
          nextToken() {
            super.nextToken();
            if (this.type === tt.eof)
              recoverable = true;
          }
          raise(pos, message) {
            switch (message) {
              case 'Unterminated template':
              case 'Unterminated comment':
                recoverable = true;
                break;

              case 'Unterminated string constant':
                const token = this.input.slice(this.lastTokStart, this.pos);
                // See https://www.ecma-international.org/ecma-262/#sec-line-terminators
                if (/\\(?:\r\n?|\n|\u2028|\u2029)$/.test(token)) {
                  recoverable = true;
                }
            }
            super.raise(pos, message);
          }
        };
      }
    );

  // Try to parse the code with acorn.  If the parse fails, ignore the acorn
  // error and return the recoverable status.
  try {
    RecoverableParser.parse(code, { ecmaVersion: 11 });

    // Odd case: the underlying JS engine (V8, Chakra) rejected this input
    // but Acorn detected no issue.  Presume that additional text won't
    // address this issue.
    return false;
  } catch {
    return recoverable;
  }
}

// Appends the preview of the result
// to the tty.
function appendPreview(repl, result, cursorTo, clearScreenDown) {
  repl.previewResult = `\u001b[90m // ${result}\u001b[39m`;
  const line = `${repl._prompt}${repl.line} //${result}`;
  const columns = repl.output.columns;
  const hasColors = repl.output.hasColors ? repl.output.hasColors() : false;
  const s = hasColors ?
    `${repl._prompt}${repl.line}${repl.previewResult}` : line;

  // Cursor to left edge.
  cursorTo(repl.output, 0);
  clearScreenDown(repl.output);

  if (columns !== undefined) {
    repl.output.write(line.length < columns ?
      s : `${s.slice(0, columns - 3)
        .replace(/\r?\n|\r/g, '')}...\u001b[39m`);
  } else {
    repl.output.write(s);
  }

  // Move back the cursor to the original position
  cursorTo(repl.output, repl.cursor + repl._prompt.length);
}

function clearPreview(repl) {
  if (repl.previewResult !== '') {
    repl._refreshLine();
    repl.previewResult = '';
  }
}

// Called whenever a line changes
// in repl and the eager eval will be
// executed against the line using v8 session
let readline;
function makePreview(repl, eagerSession, eagerEvalContextId, line) {
  const lazyReadline = () => {
    if (!readline) readline = require('readline');
    return readline;
  };

  const { cursorTo, clearScreenDown } = lazyReadline();

  clearPreview(repl);

  eagerSession.post('Runtime.evaluate', {
    expression: line.toString(),
    generatePreview: true,
    throwOnSideEffect: true,
    timeout: 500,
    executionContextId: eagerEvalContextId
  }, (error, previewResult) => {

    if (error) {
      debug(`Error while generating preview ${error}`);
      return;
    }

    if (undefined !== previewResult.result.value) {
      const value = util.inspect(previewResult.result.value);
      appendPreview(repl, value, cursorTo, clearScreenDown);
      return;
    }


    // If there is no exception and we got
    // objectId in the result, stringify it
    // using inspect via Runtime.callFunctionOn
    if (!previewResult.exceptionDetails && previewResult.result.objectId) {
      eagerSession.post('Runtime.callFunctionOn', {
        functionDeclaration:
          'function(arg) { return util.inspect(arg) }',
        arguments: [previewResult.result],
        executionContextId: eagerEvalContextId,
        returnByValue: true,
      }, (err, result) => {
        if (!err) {
          appendPreview(repl, result.result.value,
                        cursorTo, clearScreenDown);
        } else {
          debug('eager eval error', err);
        }
      });
    }
  });
}


module.exports = {
  isRecoverableError,
  kStandaloneREPL: Symbol('kStandaloneREPL'),
  makePreview
};
