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

const {
  commonPrefix
} = require('internal/readline/utils');

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
    return { showPreview() {}, clearPreview() {} };
  }

  let inputPreview = null;
  let lastInputPreview = '';

  let previewCompletionCounter = 0;
  let completionPreview = null;

  const clearPreview = () => {
    if (inputPreview !== null) {
      moveCursor(repl.output, 0, 1);
      clearLine(repl.output);
      moveCursor(repl.output, 0, -1);
      lastInputPreview = inputPreview;
      inputPreview = null;
    }
    if (completionPreview !== null) {
      // Prevent cursor moves if not necessary!
      const move = repl.line.length !== repl.cursor;
      if (move) {
        cursorTo(repl.output, repl._prompt.length + repl.line.length);
      }
      clearLine(repl.output, 1);
      if (move) {
        cursorTo(repl.output, repl._prompt.length + repl.cursor);
      }
      completionPreview = null;
    }
  };

  function showCompletionPreview(line, insertPreview) {
    previewCompletionCounter++;

    const count = previewCompletionCounter;

    repl.completer(line, (error, data) => {
      // Tab completion might be async and the result might already be outdated.
      if (count !== previewCompletionCounter) {
        return;
      }

      if (error) {
        debug('Error while generating completion preview', error);
        return;
      }

      // Result and the text that was completed.
      const [rawCompletions, completeOn] = data;

      if (!rawCompletions || rawCompletions.length === 0) {
        return;
      }

      // If there is a common prefix to all matches, then apply that portion.
      const completions = rawCompletions.filter((e) => e);
      const prefix = commonPrefix(completions);

      // No common prefix found.
      if (prefix.length <= completeOn.length) {
        return;
      }

      const suffix = prefix.slice(completeOn.length);

      // TODO(BridgeAR): Fix me. This should not be necessary. See similar
      // comment in `showPreview()`.
      if (repl.line.length + repl._prompt.length + suffix > repl.columns) {
        return;
      }

      if (insertPreview) {
        repl._insertString(suffix);
        return;
      }

      completionPreview = suffix;

      const result = repl.useColors ?
        `\u001b[90m${suffix}\u001b[39m` :
        ` // ${suffix}`;

      if (repl.line.length !== repl.cursor) {
        cursorTo(repl.output, repl._prompt.length + repl.line.length);
      }
      repl.output.write(result);
      cursorTo(repl.output, repl._prompt.length + repl.cursor);
    });
  }

  // This returns a code preview for arbitrary input code.
  function getInputPreview(input, callback) {
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

  const showPreview = () => {
    // Prevent duplicated previews after a refresh.
    if (inputPreview !== null) {
      return;
    }

    const line = repl.line.trim();

    // Do not preview in case the line only contains whitespace.
    if (line === '') {
      return;
    }

    // Add the autocompletion preview.
    // TODO(BridgeAR): Trigger the input preview after the completion preview.
    // That way it's possible to trigger the input prefix including the
    // potential completion suffix. To do so, we also have to change the
    // behavior of `enter` and `escape`:
    // Enter should automatically add the suffix to the current line as long as
    // escape was not pressed. We might even remove the preview in case any
    // cursor movement is triggered.
    if (typeof repl.completer === 'function') {
      const insertPreview = false;
      showCompletionPreview(repl.line, insertPreview);
    }

    // Do not preview if the command is buffered.
    if (repl[bufferSymbol]) {
      return;
    }

    getInputPreview(line, (error, inspected) => {
      // Ignore the output if the value is identical to the current line and the
      // former preview is not identical to this preview.
      if ((line === inspected && lastInputPreview !== inspected) ||
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

      inputPreview = inspected;

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
      cursorTo(repl.output, repl._prompt.length + repl.cursor);
    });
  };

  // -------------------------------------------------------------------------//
  // Replace multiple interface functions. This is required to fully support  //
  // previews without changing readlines behavior.                            //
  // -------------------------------------------------------------------------//

  // Refresh prints the whole screen again and the preview will be removed
  // during that procedure. Print the preview again. This also makes sure
  // the preview is always correct after resizing the terminal window.
  const originalRefresh = repl._refreshLine.bind(repl);
  repl._refreshLine = () => {
    inputPreview = null;
    originalRefresh();
    showPreview();
  };

  let insertCompletionPreview = true;
  // Insert the longest common suffix of the current input in case the user
  // moves to the right while already being at the current input end.
  const originalMoveCursor = repl._moveCursor.bind(repl);
  repl._moveCursor = (dx) => {
    const currentCursor = repl.cursor;
    originalMoveCursor(dx);
    if (currentCursor + dx > repl.line.length &&
        typeof repl.completer === 'function' &&
        insertCompletionPreview) {
      const insertPreview = true;
      showCompletionPreview(repl.line, insertPreview);
    }
  };

  // This is the only function that interferes with the completion insertion.
  // Monkey patch it to prevent inserting the completion when it shouldn't be.
  const originalClearLine = repl.clearLine.bind(repl);
  repl.clearLine = () => {
    insertCompletionPreview = false;
    originalClearLine();
    insertCompletionPreview = true;
  };

  return { showPreview, clearPreview };
}

module.exports = {
  isRecoverableError,
  kStandaloneREPL: Symbol('kStandaloneREPL'),
  setupPreview
};
