'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  Boolean,
  FunctionPrototypeBind,
  MathMin,
  Promise,
  RegExpPrototypeExec,
  SafeSet,
  SafeStringIterator,
  StringPrototypeCharCodeAt,
  StringPrototypeIndexOf,
  StringPrototypeLastIndexOf,
  StringPrototypeReplaceAll,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
  Symbol,
} = primordials;

const { getLazy } = require('internal/util');

const { getReplInspector } = require('internal/repl/inspector');

const {
  clearLine,
  clearScreenDown,
  cursorTo,
  moveCursor,
} = require('internal/readline/callbacks');

const {
  kIsMultiline,
  kSetLine,
} = require('internal/readline/interface');

const {
  commonPrefix,
  kSubstringSearch,
} = require('internal/readline/utils');

const {
  getStringWidth,
  inspect,
} = require('internal/util/inspect');

const CJSModule = require('internal/modules/cjs/loader').Module;

const vm = require('vm');

let debug = require('internal/util/debuglog').debuglog('repl', (fn) => {
  debug = fn;
});

const previewOptions = {
  colors: false,
  depth: 1,
  showHidden: false,
};

const REPL_MODE_STRICT = Symbol('repl-strict');

// If the error is that we've unexpectedly ended the input,
// then let the user try to recover by adding more input.
// Note: `e` (the original exception) is not used by the current implementation,
// but may be needed in the future.
function isRecoverableError(e, code) {
  // For similar reasons as `defaultEval`, wrap expressions starting with a
  // curly brace with parenthesis.  Note: only the open parenthesis is added
  // here as the point is to test for potentially valid but incomplete
  // expressions.
  if (RegExpPrototypeExec(/^\s*\{/, code) !== null &&
      isRecoverableError(e, `(${code}`))
    return true;

  const { tokTypes: tt, Parser: AcornParser } =
    require('internal/deps/acorn/acorn/dist/acorn');

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

              case 'Unterminated string constant': {
                const token = StringPrototypeSlice(this.input,
                                                   this.lastTokStart, this.pos);
                // See https://www.ecma-international.org/ecma-262/#sec-line-terminators
                if (RegExpPrototypeExec(/\\(?:\r\n?|\n|\u2028|\u2029)$/,
                                        token) !== null) {
                  recoverable = true;
                }
              }
            }
            super.raise(pos, message);
          }
        };
      },
    );

  // Try to parse the code with acorn.  If the parse fails, ignore the acorn
  // error and return the recoverable status.
  try {
    RecoverableParser.parse(code, { ecmaVersion: 'latest' });

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
  let previewToken = 0;

  let previewCompletionCounter = 0;
  let completionPreview = null;
  let pendingCompletionInsert = false;

  let hasCompletions = false;

  let escaped = null;

  function getPreviewPos() {
    const displayPos = repl._getDisplayPos(`${repl.getPrompt()}${repl.line}`);
    const cursorPos = repl.line.length !== repl.cursor ?
      repl.getCursorPos() :
      displayPos;
    return { displayPos, cursorPos };
  }

  function isCursorAtInputEnd() {
    const { cursorPos, displayPos } = getPreviewPos();
    return cursorPos.rows === displayPos.rows &&
           cursorPos.cols === displayPos.cols;
  }

  const clearPreview = (key) => {
    // Invalidate any preview computation that is still in flight.
    previewToken++;
    if (inputPreview !== null) {
      const { displayPos, cursorPos } = getPreviewPos();
      const rows = displayPos.rows - cursorPos.rows + 1;
      moveCursor(repl.output, 0, rows);
      clearLine(repl.output);
      moveCursor(repl.output, 0, -rows);
      inputPreview = null;
    }
    if (completionPreview !== null) {
      // Prevent cursor moves if not necessary!
      const move = repl.line.length !== repl.cursor;
      let pos, rows;
      if (move) {
        pos = getPreviewPos();
        cursorTo(repl.output, pos.displayPos.cols);
        rows = pos.displayPos.rows - pos.cursorPos.rows;
        moveCursor(repl.output, 0, rows);
      }
      const totalLine = `${repl.getPrompt()}${repl.line}${completionPreview}`;
      const newPos = repl._getDisplayPos(totalLine);
      // Minimize work for the terminal. It is enough to clear the right part of
      // the current line in case the preview is visible on a single line.
      if (newPos.rows === 0 || (pos && pos.displayPos.rows === newPos.rows)) {
        clearLine(repl.output, 1);
      } else {
        clearScreenDown(repl.output);
      }
      if (move) {
        cursorTo(repl.output, pos.cursorPos.cols);
        moveCursor(repl.output, 0, -rows);
      }
      if (!key.ctrl && !key.shift) {
        if (key.name === 'escape') {
          if (escaped === null && key.meta) {
            escaped = repl.line;
          }
        } else if ((key.name === 'return' || key.name === 'enter') &&
                   !key.meta &&
                   escaped !== repl.line &&
                   isCursorAtInputEnd()) {
          repl._insertString(completionPreview);
        }
      }
      completionPreview = null;
    }
    if (escaped !== repl.line) {
      escaped = null;
    }
  };

  async function showCompletionPreview(line, insertPreview) {
    previewCompletionCounter++;

    const count = previewCompletionCounter;

    let data;
    try {
      data = await new Promise((resolve, reject) => {
        repl.completer(line, (error, result) => {
          if (error) {
            reject(error);
          } else {
            resolve(result);
          }
        });
      });
    } catch (error) {
      debug('Error while generating completion preview', error);
      return;
    }

    if (count !== previewCompletionCounter) {
      return;
    }

    // Result and the text that was completed.
    const { 0: rawCompletions, 1: completeOn } = data;

    if (!rawCompletions || rawCompletions.length === 0) {
      return;
    }

    hasCompletions = true;

    // If there is a common prefix to all matches, then apply that portion.
    const completions = ArrayPrototypeFilter(rawCompletions, Boolean);
    const prefix = commonPrefix(completions);

    // No common prefix found.
    if (prefix.length <= completeOn.length) {
      return;
    }

    const suffix = StringPrototypeSlice(prefix, completeOn.length);

    if (insertPreview) {
      repl._insertString(suffix);
      return;
    }

    completionPreview = suffix;

    const result = repl.useColors ?
      `\u001b[90m${suffix}\u001b[39m` :
      ` // ${suffix}`;

    const { cursorPos, displayPos } = getPreviewPos();
    if (repl.line.length !== repl.cursor) {
      cursorTo(repl.output, displayPos.cols);
      moveCursor(repl.output, 0, displayPos.rows - cursorPos.rows);
    }
    repl.output.write(result);
    cursorTo(repl.output, cursorPos.cols);
    const totalLine = `${repl.getPrompt()}${repl.line}${suffix}`;
    const newPos = repl._getDisplayPos(totalLine);
    const rows = newPos.rows - cursorPos.rows - (newPos.cols === 0 ? 1 : 0);
    moveCursor(repl.output, 0, -rows);
  }

  function isInStrictMode(repl) {
    return repl.replMode === REPL_MODE_STRICT || ArrayPrototypeIncludes(
      ArrayPrototypeMap(process.execArgv,
                        (e) => StringPrototypeReplaceAll(
                          StringPrototypeToLowerCase(e),
                          '_',
                          '-',
                        )),
      '--use-strict');
  }

  // This returns a code preview for arbitrary input code.
  async function getInputPreview(input, allowWrap) {
    // For similar reasons as `defaultEval`, wrap expressions starting with a
    // curly brace with parenthesis.
    let didWrap = false;
    if (allowWrap && input[0] === '{' && input[input.length - 1] !== ';' && isValidSyntax(input)) {
      input = `(${input})`;
      didWrap = true;
    }

    const inspector = getReplInspector(repl);
    let preview;
    try {
      preview = await inspector.post('Runtime.evaluate', {
        expression: input,
        throwOnSideEffect: true,
        timeout: 333,
        contextId: repl[contextSymbol],
      });
    } catch {
      return { __proto__: null, inspected: null, didWrap };
    }
    const { result } = preview;
    if (result.value !== undefined) {
      return { __proto__: null, inspected: inspect(result.value, previewOptions), didWrap };
    }
    // Ignore EvalErrors, SyntaxErrors and ReferenceErrors. It is not clear
    // where they came from and if they are recoverable or not. Other errors
    // may be inspected.
    if (preview.exceptionDetails &&
        (result.className === 'EvalError' ||
         result.className === 'SyntaxError' ||
         // Report ReferenceError in case the strict mode is active
         // for input that has no completions.
         (result.className === 'ReferenceError' &&
          (hasCompletions || !isInStrictMode(repl))))) {
      return { __proto__: null, inspected: null, didWrap };
    }
    if (result.objectId) {
      // The writer options might change and have influence on the inspect
      // output. The user might change e.g., `showProxy`, `getters` or
      // `showHidden`. Use `inspect` instead of `JSON.stringify` to keep
      // `Infinity` and similar intact.
      const inspectOptions = inspect({
        ...repl.writer.options,
        colors: false,
        depth: 1,
        compact: true,
        breakLength: Infinity,
      }, previewOptions);
      try {
        const fnResult = await inspector.post('Runtime.callFunctionOn', {
          functionDeclaration:
            `(v) =>
                  Reflect
                  .getOwnPropertyDescriptor(globalThis, 'util')
                  .get().inspect(v, ${inspectOptions})`,
          objectId: result.objectId,
          arguments: [result],
        });
        return { __proto__: null, inspected: fnResult.result.value, didWrap };
      } catch {
        return { __proto__: null, inspected: null, didWrap };
      }
    }
    // Either not serializable or undefined.
    return { __proto__: null, inspected: result.unserializableValue || result.type, didWrap };
  }

  const showPreview = async (showCompletion = true) => {
    const acceptCompletion = pendingCompletionInsert;
    pendingCompletionInsert = false;

    // Prevent previews after a refresh, in a multiline command, or when
    // previews are disabled.
    if (inputPreview !== null ||
        repl[kIsMultiline] ||
        !repl.isCompletionEnabled ||
        !process.features.inspector) {
      return;
    }

    const line = StringPrototypeTrim(repl.line);

    // Do not preview in case the line only contains whitespace.
    if (line === '') {
      return;
    }

    hasCompletions = false;

    const token = ++previewToken;

    const renderInputPreview = (inspected) => {
      if (inspected == null) {
        return;
      }

      // Ignore the output if the value is identical to the current line.
      if (line === inspected) {
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
      let maxColumns = MathMin(repl.columns, 250);

      // Support unicode characters of width other than one by checking the
      // actual width.
      if (inspected.length * 2 >= maxColumns &&
          getStringWidth(inspected) > maxColumns) {
        maxColumns -= 4 + (repl.useColors ? 0 : 3);
        let res = '';
        for (const char of new SafeStringIterator(inspected)) {
          maxColumns -= getStringWidth(char);
          if (maxColumns < 0)
            break;
          res += char;
        }
        inspected = `${res}...`;
      }

      // Line breaks are very rare and probably only occur in case of error
      // messages with line breaks.
      const lineBreakMatch = RegExpPrototypeExec(/[\r\n\v]/, inspected);
      if (lineBreakMatch !== null) {
        inspected = `${StringPrototypeSlice(inspected, 0, lineBreakMatch.index)}`;
      }

      const result = repl.useColors ?
        `\u001b[90m${inspected}\u001b[39m` :
        `// ${inspected}`;

      const { cursorPos, displayPos } = getPreviewPos();
      const rows = displayPos.rows - cursorPos.rows;
      // Moves one line below all the user lines
      moveCursor(repl.output, 0, rows);
      // Writes the preview there
      repl.output.write(`\n${result}`);

      // Go back to the horizontal position of the cursor
      cursorTo(repl.output, cursorPos.cols);
      // Go back to the vertical position of the cursor
      moveCursor(repl.output, 0, -rows - 1);
    };

    // Add the autocompletion preview first, so the resolved completion can be
    // appended to the previewed expression below.
    let insertedCompletion = false;
    if (showCompletion) {
      insertedCompletion = acceptCompletion;
      await showCompletionPreview(repl.line, acceptCompletion);
    }

    // Bail if a newer keystroke superseded this preview while the completion
    // was being resolved, or if the command is buffered.
    if (token !== previewToken || repl[bufferSymbol]) {
      return;
    }

    // Re-read the input in case the completion was just inserted above.
    let previewLine = insertedCompletion ? StringPrototypeTrim(repl.line) : line;

    if (completionPreview !== null &&
        isCursorAtInputEnd() &&
        escaped !== repl.line) {
      previewLine += completionPreview;
    }

    const preview = await getInputPreview(previewLine, true);
    let { inspected } = preview;

    if (inspected == null && preview.didWrap) {
      ({ inspected } = await getInputPreview(previewLine, false));
    }

    // Ignore the result if a newer keystroke superseded this preview.
    if (token !== previewToken) {
      return;
    }

    renderInputPreview(inspected);
  };

  // -------------------------------------------------------------------------//
  // Replace multiple interface functions. This is required to fully support  //
  // previews without changing readlines behavior.                            //
  // -------------------------------------------------------------------------//

  // Refresh prints the whole screen again and the preview will be removed
  // during that procedure. Print the preview again. This also makes sure
  // the preview is always correct after resizing the terminal window.
  const originalRefresh = FunctionPrototypeBind(repl._refreshLine, repl);
  repl._refreshLine = () => {
    inputPreview = null;
    originalRefresh();
    showPreview();
  };

  let insertCompletionPreview = true;
  // Insert the longest common suffix of the current input in case the user
  // moves to the right while already being at the current input end.
  const originalMoveCursor = FunctionPrototypeBind(repl._moveCursor, repl);
  repl._moveCursor = (dx) => {
    const currentCursor = repl.cursor;
    originalMoveCursor(dx);
    if (currentCursor + dx > repl.line.length &&
        typeof repl.completer === 'function' &&
        insertCompletionPreview) {
      pendingCompletionInsert = true;
    }
  };

  // This is the only function that interferes with the completion insertion.
  // Monkey patch it to prevent inserting the completion when it shouldn't be.
  const originalClearLine = FunctionPrototypeBind(repl.clearLine, repl);
  repl.clearLine = () => {
    insertCompletionPreview = false;
    originalClearLine();
    insertCompletionPreview = true;
  };

  return { showPreview, clearPreview };
}

function setupReverseSearch(repl) {
  // Simple terminals can't use reverse search.
  if (process.env.TERM === 'dumb') {
    return { reverseSearch() { return false; } };
  }

  const alreadyMatched = new SafeSet();
  const labels = {
    r: 'bck-i-search: ',
    s: 'fwd-i-search: ',
  };
  let isInReverseSearch = false;
  let historyIndex = -1;
  let input = '';
  let cursor = -1;
  let dir = 'r';
  let lastMatch = -1;
  let lastCursor = -1;
  let promptPos;

  function checkAndSetDirectionKey(keyName) {
    if (!labels[keyName]) {
      return false;
    }
    if (dir !== keyName) {
      // Reset the already matched set in case the direction is changed. That
      // way it's possible to find those entries again.
      alreadyMatched.clear();
      dir = keyName;
    }
    return true;
  }

  function goToNextHistoryIndex() {
    // Ignore this entry for further searches and continue to the next
    // history entry.
    alreadyMatched.add(repl.history[historyIndex]);
    historyIndex += dir === 'r' ? 1 : -1;
    cursor = -1;
  }

  function search() {
    // Just print an empty line in case the user removed the search parameter.
    if (input === '') {
      print(repl.line, `${labels[dir]}_`);
      return;
    }
    // Fix the bounds in case the direction has changed in the meanwhile.
    if (dir === 'r') {
      if (historyIndex < 0) {
        historyIndex = 0;
      }
    } else if (historyIndex >= repl.history.length) {
      historyIndex = repl.history.length - 1;
    }
    // Check the history entries until a match is found.
    while (historyIndex >= 0 && historyIndex < repl.history.length) {
      let entry = repl.history[historyIndex];
      // Visualize all potential matches only once.
      if (alreadyMatched.has(entry)) {
        historyIndex += dir === 'r' ? 1 : -1;
        continue;
      }
      // Match the next entry either from the start or from the end, depending
      // on the current direction.
      if (dir === 'r') {
        // Update the cursor in case it's necessary.
        if (cursor === -1) {
          cursor = entry.length;
        }
        cursor = StringPrototypeLastIndexOf(entry, input, cursor - 1);
      } else {
        cursor = StringPrototypeIndexOf(entry, input, cursor + 1);
      }
      // Match not found.
      if (cursor === -1) {
        goToNextHistoryIndex();
      // Match found.
      } else {
        if (repl.useColors) {
          const start = StringPrototypeSlice(entry, 0, cursor);
          const end = StringPrototypeSlice(entry, cursor + input.length);
          entry = `${start}\x1B[4m${input}\x1B[24m${end}`;
        }
        print(entry, `${labels[dir]}${input}_`, cursor);
        lastMatch = historyIndex;
        lastCursor = cursor;
        // Explicitly go to the next history item in case no further matches are
        // possible with the current entry.
        if ((dir === 'r' && cursor === 0) ||
            (dir === 's' && entry.length === cursor + input.length)) {
          goToNextHistoryIndex();
        }
        return;
      }
    }
    print(repl.line, `failed-${labels[dir]}${input}_`);
  }

  function print(outputLine, inputLine, cursor = repl.cursor) {
    // TODO(BridgeAR): Resizing the terminal window hides the overlay. To fix
    // that, readline must be aware of this information. It's probably best to
    // add a couple of properties to readline that allow to do the following:
    // 1. Add arbitrary data to the end of the current line while not counting
    //    towards the line. This would be useful for the completion previews.
    // 2. Add arbitrary extra lines that do not count towards the regular line.
    //    This would be useful for both, the input preview and the reverse
    //    search. It might be combined with the first part?
    // 3. Add arbitrary input that is "on top" of the current line. That is
    //    useful for the reverse search.
    // 4. To trigger the line refresh, functions should be used to pass through
    //    the information. Alternatively, getters and setters could be used.
    //    That might even be more elegant.
    // The data would then be accounted for when calling `_refreshLine()`.
    // This function would then look similar to:
    //   repl.overlay(outputLine);
    //   repl.addTrailingLine(inputLine);
    //   repl.setCursor(cursor);
    // More potential improvements: use something similar to stream.cork().
    // Multiple cursor moves on the same tick could be prevented in case all
    // writes from the same tick are combined and the cursor is moved at the
    // tick end instead of after each operation.
    let rows = 0;
    if (lastMatch !== -1) {
      const line = StringPrototypeSlice(repl.history[lastMatch], 0, lastCursor);
      rows = repl._getDisplayPos(`${repl.getPrompt()}${line}`).rows;
      cursorTo(repl.output, promptPos.cols);
    } else if (isInReverseSearch && repl.line !== '') {
      rows = repl.getCursorPos().rows;
      cursorTo(repl.output, promptPos.cols);
    }
    if (rows !== 0)
      moveCursor(repl.output, 0, -rows);

    if (isInReverseSearch) {
      clearScreenDown(repl.output);
      repl.output.write(`${outputLine}\n${inputLine}`);
    } else {
      repl.output.write(`\n${inputLine}`);
    }

    lastMatch = -1;

    // To know exactly how many rows we have to move the cursor back we need the
    // cursor rows, the output rows and the input rows.
    const prompt = repl.getPrompt();
    const cursorLine = prompt + StringPrototypeSlice(outputLine, 0, cursor);
    const cursorPos = repl._getDisplayPos(cursorLine);
    const outputPos = repl._getDisplayPos(`${prompt}${outputLine}`);
    const inputPos = repl._getDisplayPos(inputLine);
    const inputRows = inputPos.rows - (inputPos.cols === 0 ? 1 : 0);

    rows = -1 - inputRows - (outputPos.rows - cursorPos.rows);

    moveCursor(repl.output, 0, rows);
    cursorTo(repl.output, cursorPos.cols);
  }

  function reset(string) {
    isInReverseSearch = string !== undefined;

    // In case the reverse search ends and a history entry is found, reset the
    // line to the found entry.
    if (!isInReverseSearch) {
      if (lastMatch !== -1) {
        repl[kSetLine](repl.history[lastMatch]);
        repl.cursor = lastCursor;
        repl.historyIndex = lastMatch;
      }

      lastMatch = -1;

      // Clear screen and write the current repl.line before exiting.
      cursorTo(repl.output, promptPos.cols);
      moveCursor(repl.output, 0, promptPos.rows);
      clearScreenDown(repl.output);
      if (repl.line !== '') {
        repl.output.write(repl.line);
        if (repl.line.length !== repl.cursor) {
          const { cols, rows } = repl.getCursorPos();
          cursorTo(repl.output, cols);
          moveCursor(repl.output, 0, rows);
        }
      }
    }

    input = string || '';
    cursor = -1;
    historyIndex = repl.historyIndex;
    alreadyMatched.clear();
  }

  function reverseSearch(string, key) {
    if (!isInReverseSearch) {
      if (key.ctrl && checkAndSetDirectionKey(key.name)) {
        historyIndex = repl.historyIndex;
        promptPos = repl._getDisplayPos(`${repl.getPrompt()}`);
        print(repl.line, `${labels[dir]}_`);
        isInReverseSearch = true;
      }
    } else if (key.ctrl && checkAndSetDirectionKey(key.name)) {
      search();
    } else if (key.name === 'backspace' ||
        (key.ctrl && (key.name === 'h' || key.name === 'w'))) {
      reset(StringPrototypeSlice(input, 0, input.length - 1));
      search();
      // Special handle <ctrl> + c and escape. Those should only cancel the
      // reverse search. The original line is visible afterwards again.
    } else if ((key.ctrl && key.name === 'c') || key.name === 'escape') {
      lastMatch = -1;
      reset();
      return true;
      // End search in case either enter is pressed or if any non-reverse-search
      // key (combination) is pressed.
    } else if (key.ctrl ||
               key.meta ||
               key.name === 'return' ||
               key.name === 'enter' ||
               typeof string !== 'string' ||
               string === '') {
      reset();
      repl[kSubstringSearch] = '';
    } else {
      reset(`${input}${string}`);
      search();
    }
    return isInReverseSearch;
  }

  return { reverseSearch };
}

const startsWithBraceRegExp = /^\s*{/;
const endsWithSemicolonRegExp = /;\s*$/;
function isValidSyntax(input) {
  const { Parser: AcornParser } =
    require('internal/deps/acorn/acorn/dist/acorn');
  try {
    AcornParser.parse(input, {
      ecmaVersion: 'latest',
      allowAwaitOutsideFunction: true,
    });
    return true;
  } catch {
    try {
      AcornParser.parse(`_=${input}`, {
        ecmaVersion: 'latest',
        allowAwaitOutsideFunction: true,
      });
      return true;
    } catch {
      return false;
    }
  }
}

/**
 * Checks if some provided code represents an object literal.
 * This is helpful to prevent confusing repl code evaluations where
 * strings such as `{ a : 1 }` would get interpreted as block statements
 * rather than object literals.
 * @param {string} code the code to check
 * @returns {boolean} true if the code represents an object literal, false otherwise
 */
function isObjectLiteral(code) {
  return RegExpPrototypeExec(startsWithBraceRegExp, code) !== null &&
    RegExpPrototypeExec(endsWithSemicolonRegExp, code) === null;
}

const kContextId = Symbol('contextId');

const path = require('path');

function fixReplRequire(replModule) {
  try {
    // Hack for require.resolve("./relative") to work properly.
    replModule.filename = path.resolve('repl');
  } catch {
    // path.resolve('repl') fails when the current working directory has been
    // deleted.  Fall back to the directory name of the (absolute) executable
    // path.  It's not really correct but what are the alternatives?
    const dirname = path.dirname(process.execPath);
    replModule.filename = path.resolve(dirname, 'repl');
  }

  // Hack for repl require to work properly with node_modules folders
  replModule.paths = CJSModule._nodeModulePaths(replModule.filename);
}

let nextREPLResourceNumber = 1;
// This prevents v8 code cache from getting confused and using a different
// cache from a resource of the same name
function getREPLResourceName() {
  return `REPL${nextREPLResourceNumber++}`;
}

// Creating a new context is expensive, so only do it on first use.
const getGlobalBuiltins = getLazy(() =>
  new SafeSet(vm.runInNewContext('Object.getOwnPropertyNames(globalThis)')));

let _builtinLibs = ArrayPrototypeFilter(
  CJSModule.builtinModules,
  (e) => e[0] !== '_' && !StringPrototypeStartsWith(e, 'node:'),
);

// Note: the `getReplBuiltinLibs` and `setReplBuiltinLibs` are functions used to provide getters and
//       setters for the `builtinModules` and `_builtinLibs` properties of the repl module and for making
//       sure that all internal repl modules share the same value, which can potentially be updated by users.
//       Also note that both `repl.builtinModules` and `repl._builtinLibs` are deprecated, once such properties
//       are removed these two functions should also be removed as no longer necessary.

function getReplBuiltinLibs() {
  return _builtinLibs;
}

function setReplBuiltinLibs(value) {
  _builtinLibs = value;
}


// Syntax highlighting uses util.styleText to dogfood Node.js core's
// own coloring API. The color names align with util.inspect.styles
// (e.g. 'magenta' for keywords, 'green' for strings, 'yellow' for numbers).
const { styleText } = require('util');
const kStyleOpts = { __proto__: null, validateStream: false };

/**
 * Wraps text with an ANSI color via util.styleText.
 * Uses the fast path (validateStream: false) to skip stream checks.
 * @param {string} colorName The color name (e.g. 'magenta', 'green').
 * @param {string} text The text to colorize.
 * @returns {string} The colorized text.
 */
function style(colorName, text) {
  return styleText(colorName, text, kStyleOpts);
}

/**
 * Applies ANSI syntax highlighting to a JavaScript code string.
 * Uses the Acorn tokenizer to identify token types and wrap them
 * in appropriate ANSI color escape sequences.
 * @param {string} code The JavaScript code string to highlight.
 * @param {boolean} useColors Whether to apply color codes.
 * @returns {string} The highlighted code string, or the original if
 *   useColors is false or tokenization fails.
 */
function syntaxHighlight(code, useColors) {
  if (!useColors || code === '') {
    return code;
  }

  const {
    tokTypes: tt,
    tokenizer: createTokenizer,
  } = require('internal/deps/acorn/acorn/dist/acorn');

  const parts = [];
  let lastEnd = 0;

  try {
    const tokenizer = createTokenizer(code, {
      ecmaVersion: 'latest',
      allowAwaitOutsideFunction: true,
      allowImportExportEverywhere: true,
      allowReturnOutsideFunction: true,
      allowSuperOutsideMethod: true,
    });

    // Collect comments via the onComment callback since the tokenizer
    // iterator does not yield comment tokens.
    const comments = [];
    tokenizer.options.onComment = (block, text, start, end) => {
      ArrayPrototypePush(comments, { start, end });
    };

    for (const token of tokenizer) {
      // First, handle any comments that occur before this token.
      while (comments.length > 0 && comments[0].start < token.start) {
        const comment = comments[0];
        // Add any text between last position and comment start.
        if (comment.start > lastEnd) {
          ArrayPrototypePush(
            parts, StringPrototypeSlice(code, lastEnd, comment.start));
        }
        ArrayPrototypePush(parts,
                           style('gray',
                                 StringPrototypeSlice(code, comment.start,
                                                      comment.end)));
        lastEnd = comment.end;
        comments.shift();
      }

      // Add any unhighlighted text between the last token and this one.
      if (token.start > lastEnd) {
        ArrayPrototypePush(
          parts, StringPrototypeSlice(code, lastEnd, token.start));
      }

      const tokenText = StringPrototypeSlice(code, token.start, token.end);
      let colorName;

      if (token.type.keyword) {
        // Keywords: const, let, var, function, if, else, etc.
        // Also includes true, false, null which have keyword set.
        const kw = token.type.keyword;
        if (kw === 'true' || kw === 'false' || kw === 'null') {
          colorName = 'yellow';
        } else {
          colorName = 'magenta';
        }
      } else if (token.type === tt.num) {
        colorName = 'yellow';
      } else if (token.type === tt.string) {
        colorName = 'green';
      } else if (token.type === tt.template || token.type === tt.backQuote ||
                 token.type === tt.dollarBraceL ||
                 token.type === tt.invalidTemplate) {
        colorName = 'green';
      } else if (token.type === tt.regexp) {
        colorName = 'red';
      } else if (token.type === tt.name &&
                 (tokenText === 'undefined' || tokenText === 'NaN' ||
                  tokenText === 'Infinity')) {
        colorName = 'yellow';
      } else if (token.type === tt.name &&
                 (tokenText === 'let' || tokenText === 'const' ||
                  tokenText === 'async' || tokenText === 'await' ||
                  tokenText === 'class' || tokenText === 'yield' ||
                  tokenText === 'of' || tokenText === 'static' ||
                  tokenText === 'get' || tokenText === 'set')) {
        colorName = 'magenta';
      }

      if (colorName) {
        ArrayPrototypePush(parts, style(colorName, tokenText));
      } else {
        ArrayPrototypePush(parts, tokenText);
      }

      lastEnd = token.end;
    }

    // Handle any trailing comments.
    while (comments.length > 0) {
      const comment = comments[0];
      if (comment.start > lastEnd) {
        ArrayPrototypePush(
          parts, StringPrototypeSlice(code, lastEnd, comment.start));
      }
      ArrayPrototypePush(parts,
                         style('gray',
                               StringPrototypeSlice(code, comment.start,
                                                    comment.end)));
      lastEnd = comment.end;
      comments.shift();
    }

    // Append any remaining code after the last token.
    if (lastEnd < code.length) {
      ArrayPrototypePush(parts, StringPrototypeSlice(code, lastEnd));
    }
  } catch {
    // If tokenization fails (e.g., due to incomplete input in REPL),
    // fall back to returning the code without highlighting.
    return code;
  }

  return ArrayPrototypeJoin(parts, '');
}

/**
 * Hooks into the REPL's _writeToOutput to apply syntax highlighting
 * to the current input line during _refreshLine. This ensures
 * the user sees highlighted code without affecting the internal
 * `this.line` property (which must remain plain text for correct
 * cursor positioning and evaluation).
 * @param {REPLServer} repl The REPL instance.
 * @param {boolean} active Whether syntax highlighting should be enabled.
 */
function setupSyntaxHighlighting(repl, active) {
  if (process.env.TERM === 'dumb' || !active || !repl.useColors) {
    return;
  }

  const originalRefreshLine =
    FunctionPrototypeBind(repl._refreshLine, repl);

  let isRefreshing = false;

  // Override _writeToOutput to apply highlighting during refresh.
  const originalWriteToOutput =
    FunctionPrototypeBind(repl._writeToOutput, repl);
  repl._writeToOutput = (stringToWrite) => {
    if (isRefreshing && stringToWrite.length > 0) {
      const prompt = repl.getPrompt() || repl._prompt || '';
      // Only highlight when the string starts with the prompt (i.e.,
      // this is the main line being written during _refreshLine).
      if (StringPrototypeStartsWith(stringToWrite, prompt) &&
          stringToWrite !== prompt) {
        const code = StringPrototypeSlice(stringToWrite, prompt.length);
        const highlighted = syntaxHighlight(code, true);
        return originalWriteToOutput(prompt + highlighted);
      }
    }
    return originalWriteToOutput(stringToWrite);
  };

  // Wrap _refreshLine to set the isRefreshing flag.
  repl._refreshLine = () => {
    isRefreshing = true;
    try {
      originalRefreshLine();
    } finally {
      isRefreshing = false;
    }
  };
}

/**
 * Sets up automatic indentation for multiline REPL input.
 * After a newline is inserted (due to Recoverable error), this
 * calculates the appropriate indentation level based on open
 * braces/brackets/parentheses and inserts the correct number of spaces.
 * @param {REPLServer} repl The REPL instance.
 */
function setupAutoIndent(repl) {
  if (!repl.terminal) {
    return;
  }

  const kIndent = '  '; // 2-space indentation (Node.js convention)

  /**
   * Calculates the net depth change from unmatched braces, brackets,
   * and parentheses in a given code string.
   * @returns {number} The net depth change.
   */
  function calculateDepth(code) {
    let depth = 0;
    let inString = false;
    let stringChar = '';
    let inTemplate = false;
    let inLineComment = false;
    let inBlockComment = false;
    let escaped = false;

    for (let i = 0; i < code.length; i++) {
      const ch = StringPrototypeCharCodeAt(code, i);

      if (escaped) {
        escaped = false;
        continue;
      }

      // Backslash escape
      if (ch === 92) { // '\\'
        escaped = true;
        continue;
      }

      // Line comments
      if (inLineComment) {
        if (ch === 10 || ch === 13) inLineComment = false; // newline
        continue;
      }

      // Block comments
      if (inBlockComment) {
        if (ch === 42 && StringPrototypeCharCodeAt(code, i + 1) === 47) { // */
          inBlockComment = false;
          i++;
        }
        continue;
      }

      // String literals
      if (inString) {
        if (ch === StringPrototypeCharCodeAt(stringChar, 0)) {
          inString = false;
        }
        continue;
      }

      // Template literals
      if (inTemplate && ch === 96) { // backtick
        inTemplate = false;
        continue;
      }

      // Check for comment starts
      if (ch === 47) { // '/'
        const next = StringPrototypeCharCodeAt(code, i + 1);
        if (next === 47) { // '//'
          inLineComment = true;
          i++;
          continue;
        }
        if (next === 42) { // '/*'
          inBlockComment = true;
          i++;
          continue;
        }
      }

      // String start
      if (ch === 39 || ch === 34) { // ' or "
        inString = true;
        stringChar = code[i];
        continue;
      }

      // Template literal
      if (ch === 96) { // backtick
        inTemplate = true;
        continue;
      }

      // Opening delimiters
      if (ch === 123 || ch === 91 || ch === 40) { // { [ (
        depth++;
      }
      // Closing delimiters
      if (ch === 125 || ch === 93 || ch === 41) { // } ] )
        depth--;
      }
    }

    return depth;
  }

  /**
   * Gets the indentation level for the next line based on the
   * full buffered command so far.
   * @returns {string} The indentation string.
   */
  repl._getAutoIndent = function(bufferedCmd) {
    if (!bufferedCmd) return '';

    const depth = calculateDepth(bufferedCmd);
    if (depth <= 0) return '';

    let indent = '';
    for (let i = 0; i < depth; i++) {
      indent += kIndent;
    }
    return indent;
  };
}

/**
 * Sets up function signature hints. When the user types a function
 * name followed by '(', a dimmed hint showing the function's
 * parameters is displayed below the input.
 * @param {REPLServer} repl The REPL instance.
 * @param {symbol} contextSymbol The context ID symbol.
 * @param {boolean} active Whether hints should be enabled.
 * @returns {object} An object with showSignatureHint and clearSignatureHint methods.
 */
function setupSignatureHint(repl, contextSymbol, active) {
  if (process.env.TERM === 'dumb' || !active || !repl.terminal ||
      !process.features.inspector) {
    return { showSignatureHint() {}, clearSignatureHint() {} };
  }

  const clearLineOutput = clearLine;
  const cursorToOutput = cursorTo;
  const moveCursorOutput = moveCursor;

  let currentHint = null;

  function clearSignatureHint() {
    if (currentHint === null) return;

    const { cursorPos, displayPos } = getPreviewPos();
    // Move to the line below the input.
    const rows = displayPos.rows - cursorPos.rows + 1;
    moveCursorOutput(repl.output, 0, rows);
    clearLineOutput(repl.output);
    moveCursorOutput(repl.output, 0, -rows);
    currentHint = null;
  }

  function getPreviewPos() {
    const displayPos =
      repl._getDisplayPos(`${repl.getPrompt()}${repl.line}`);
    const cursorPos = repl.line.length !== repl.cursor ?
      repl.getCursorPos() : displayPos;
    return { displayPos, cursorPos };
  }

  function showSignatureHint() {
    if (currentHint !== null) return;

    const line = repl.line;
    const cursor = repl.cursor;

    // Find the function name before the opening parenthesis.
    // Look backwards from cursor for pattern: identifier(
    let parenPos = -1;
    for (let i = cursor - 1; i >= 0; i--) {
      const ch = StringPrototypeCharCodeAt(line, i);
      if (ch === 40) { // '('
        parenPos = i;
        break;
      }
      // If we hit anything other than whitespace or content after '(',
      // stop looking.
      if (ch !== 32 && ch !== 9) break; // space, tab
    }

    if (parenPos < 0) return;

    // Extract the expression before the '(' - could be dotted like
    // console.log or just a simple name.
    const nameEnd = parenPos;
    let nameStart = nameEnd;
    for (let i = nameEnd - 1; i >= 0; i--) {
      const ch = StringPrototypeCharCodeAt(line, i);
      // Allow identifier chars and dots for property access.
      if ((ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122) ||
          (ch >= 48 && ch <= 57) || ch === 95 || ch === 36 || ch === 46) {
        nameStart = i;
      } else {
        break;
      }
    }

    if (nameStart >= nameEnd) return;

    const funcName = StringPrototypeSlice(line, nameStart, nameEnd);
    if (!funcName) return;

    // Use the inspector to get the function's toString() safely.
    const inspector = getReplInspector(repl);
    inspector.post('Runtime.evaluate', {
      expression: `typeof ${funcName} === 'function' ? ` +
        `${funcName}.toString().match(/^[^{=]*\\(([^)]*)\\)/)?.[1] || '' : ''`,
      throwOnSideEffect: true,
      timeout: 200,
      contextId: repl[contextSymbol],
    }).then((preview) => {
      if (!preview?.result?.value) return;

      const params = preview.result.value;
      if (!params) return;

      const hint = `${funcName}(${params})`;
      currentHint = hint;

      const { cursorPos, displayPos } = getPreviewPos();
      const rows = displayPos.rows - cursorPos.rows;
      moveCursorOutput(repl.output, 0, rows);
      const result = repl.useColors ?
        `\n${style('gray', hint)}` :
        `\n// ${hint}`;
      repl.output.write(result);
      cursorToOutput(repl.output, cursorPos.cols);
      moveCursorOutput(repl.output, 0, -rows - 1);
    }).catch(() => { /* Inspector not available or failed, silently skip. */ });
  }

  return { showSignatureHint, clearSignatureHint };
}

function setupPreview(repl, contextSymbol, bufferSymbol, active) {
  // Simple terminals can't handle previews.
  if (process.env.TERM === 'dumb' || !active) {


module.exports = {
  REPL_MODE_SLOPPY: Symbol('repl-sloppy'),
  REPL_MODE_STRICT,
  isRecoverableError,
  kStandaloneREPL: Symbol('kStandaloneREPL'),
  setupPreview,
  setupReverseSearch,
  setupSyntaxHighlighting,
  setupAutoIndent,
  setupSignatureHint,
  syntaxHighlight,
  isObjectLiteral,
  isValidSyntax,
  kContextId,
  getREPLResourceName,
  getGlobalBuiltins,
  getReplBuiltinLibs,
  setReplBuiltinLibs,
  fixReplRequire,
};
