'use strict';

const {
  MathMin,
  Symbol,
} = primordials;

const { tokTypes: tt, Parser: AcornParser } =
  require('internal/deps/acorn/acorn/dist/acorn');
const privateMethods =
  require('internal/deps/acorn-plugins/acorn-private-methods/index');
const classFields =
  require('internal/deps/acorn-plugins/acorn-class-fields/index');
const numericSeparator =
  require('internal/deps/acorn-plugins/acorn-numeric-separator/index');
const staticClassFeatures =
  require('internal/deps/acorn-plugins/acorn-static-class-features/index');

const { sendInspectorCommand } = require('internal/util/inspector');

const {
  ERR_INSPECTOR_NOT_AVAILABLE
} = require('internal/errors').codes;

const {
  clearLine,
  cursorTo,
  moveCursor,
} = require('readline');

const { inspect } = require('util');

const debug = require('internal/util/debuglog').debuglog('repl');

const inspectOptions = {
  depth: 1,
  colors: false,
  compact: true,
  breakLength: Infinity
};
const inspectedOptions = inspect(inspectOptions, { colors: false });

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

function setupPreview(repl, contextSymbol, bufferSymbol, active) {
  // Simple terminals can't handle previews.
  if (process.env.TERM === 'dumb' || !active) {
    return { showInputPreview() {}, clearPreview() {} };
  }

  let preview = null;
  let lastPreview = '';

  const clearPreview = () => {
    if (preview !== null) {
      moveCursor(repl.output, 0, 1);
      clearLine(repl.output);
      moveCursor(repl.output, 0, -1);
      lastPreview = preview;
      preview = null;
    }
  };

  // This returns a code preview for arbitrary input code.
  function getPreviewInput(input, callback) {
    // For similar reasons as `defaultEval`, wrap expressions starting with a
    // curly brace with parenthesis.
    if (input.startsWith('{') && !input.endsWith(';')) {
      input = `(${input})`;
    }
    sendInspectorCommand((session) => {
      session.post('Runtime.evaluate', {
        expression: input,
        throwOnSideEffect: true,
        timeout: 333,
        contextId: repl[contextSymbol],
      }, (error, preview) => {
        if (error) {
          callback(error);
          return;
        }
        const { result } = preview;
        if (result.value !== undefined) {
          callback(null, inspect(result.value, inspectOptions));
        // Ignore EvalErrors, SyntaxErrors and ReferenceErrors. It is not clear
        // where they came from and if they are recoverable or not. Other errors
        // may be inspected.
        } else if (preview.exceptionDetails &&
                   (result.className === 'EvalError' ||
                     result.className === 'SyntaxError' ||
                     result.className === 'ReferenceError')) {
          callback(null, null);
        } else if (result.objectId) {
          session.post('Runtime.callFunctionOn', {
            functionDeclaration: `(v) => util.inspect(v, ${inspectedOptions})`,
            objectId: result.objectId,
            arguments: [result]
          }, (error, preview) => {
            if (error) {
              callback(error);
            } else {
              callback(null, preview.result.value);
            }
          });
        } else {
          // Either not serializable or undefined.
          callback(null, result.unserializableValue || result.type);
        }
      });
    }, () => callback(new ERR_INSPECTOR_NOT_AVAILABLE()));
  }

  const showInputPreview = () => {
    // Prevent duplicated previews after a refresh.
    if (preview !== null) {
      return;
    }

    const line = repl.line.trim();

    // Do not preview if the command is buffered or if the line is empty.
    if (repl[bufferSymbol] || line === '') {
      return;
    }

    getPreviewInput(line, (error, inspected) => {
      // Ignore the output if the value is identical to the current line and the
      // former preview is not identical to this preview.
      if ((line === inspected && lastPreview !== inspected) ||
          inspected === null) {
        return;
      }
      if (error) {
        debug('Error while generating preview', error);
        return;
      }
      // Do not preview `undefined` if colors are deactivated or explicitly
      // requested.
      if (inspected === 'undefined' &&
          (!repl.useColors || repl.ignoreUndefined)) {
        return;
      }

      preview = inspected;

      // Limit the output to maximum 250 characters. Otherwise it becomes a)
      // difficult to read and b) non terminal REPLs would visualize the whole
      // output.
      const maxColumns = MathMin(repl.columns, 250);

      if (inspected.length > maxColumns) {
        inspected = `${inspected.slice(0, maxColumns - 6)}...`;
      }
      const lineBreakPos = inspected.indexOf('\n');
      if (lineBreakPos !== -1) {
        inspected = `${inspected.slice(0, lineBreakPos)}`;
      }
      const result = repl.useColors ?
        `\u001b[90m${inspected}\u001b[39m` :
        `// ${inspected}`;

      repl.output.write(`\n${result}`);
      moveCursor(repl.output, 0, -1);
      cursorTo(repl.output, repl.cursor + repl._prompt.length);
    });
  };

  // Refresh prints the whole screen again and the preview will be removed
  // during that procedure. Print the preview again. This also makes sure
  // the preview is always correct after resizing the terminal window.
  const tmpRefresh = repl._refreshLine.bind(repl);
  repl._refreshLine = () => {
    preview = null;
    tmpRefresh();
    showInputPreview();
  };

  return { showInputPreview, clearPreview };
}

module.exports = {
  isRecoverableError,
  kStandaloneREPL: Symbol('kStandaloneREPL'),
  setupPreview
};
