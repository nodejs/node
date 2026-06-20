'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeSlice,
  ObjectDefineProperty,
  PromisePrototypeThen,
  ReflectApply,
  ReflectHas,
  RegExpPrototypeExec,
  StringPrototypeSplit,
  StringPrototypeTrim,
  Symbol,
  SyntaxError,
} = primordials;

const path = require('path');
const {
  isRecoverableError,
  isObjectLiteral,
  isValidSyntax,
  kContextId,
  REPL_MODE_STRICT,
} = require('internal/repl/utils');
const {
  kDynamicImportName,
  transformModuleSyntax,
} = require('internal/repl/transform');
const { getReplInspector, prepareContext } = require('internal/repl/inspector');

let debug = require('internal/util/debuglog').debuglog('repl', (fn) => {
  debug = fn;
});

const blankRegExp = /^\s*$/;

// Matches the first stack frame that belongs to the REPL
const internalFrameRegExp =
  /^\s*at\s.*(?:node:inspector|node:internal\/repl|node:repl)[:)]/;

// Removes the REPL's internal evaluation frames from an error stack string,
// leaving only the frames that correspond to the user's code.
function trimInternalFrames(err) {
  if (typeof err?.stack !== 'string') {
    return;
  }
  const lines = StringPrototypeSplit(err.stack, '\n');
  for (let i = 0; i < lines.length; i++) {
    if (RegExpPrototypeExec(internalFrameRegExp, lines[i]) !== null) {
      err.stack = ArrayPrototypeJoin(ArrayPrototypeSlice(lines, 0, i), '\n');
      return;
    }
  }
}

// Thrown to signal the server that the input is incomplete and a continuation
// prompt should be shown. Mirrors the public `repl.Recoverable`.
class Recoverable extends SyntaxError {
  constructor(err) {
    super();
    this.err = err;
  }
}

// Mark falsy thrown values out-of-band so the evaluator callback cannot
// mistake them for successful completion.
const kThrownValue = Symbol('kThrownValue');

/**
 * Creates the evaluation function used by a {@link REPLServer}.
 *
 * The returned function matches the historical `eval` contract
 * `(code, context, file, cb)` so it is a drop-in replacement for the old
 * `vm`-based default evaluator and for any custom evaluator a user supplies.
 * @param {object} repl the REPL server instance
 * @returns {Function} the evaluation function
 */
function createReplEval(repl) {
  const inspector = getReplInspector(repl);

  // Installs the dynamic-import helper that the transform routes `import`
  // through. It mirrors the host-defined `importModuleDynamically` callback the
  // old evaluator passed to `makeContextifyScript`, so relative specifiers
  // resolve against the working directory exactly as they did before.
  function installDynamicImport(context) {
    if (ReflectHas(context, kDynamicImportName)) {
      return;
    }
    const dynamicImport = (specifier, importAttributes) => {
      let parentURL;
      try {
        const { pathToFileURL } = require('internal/url');
        // Appending `/repl` keeps relative imports from resolving against the
        // parent of `process.cwd()`.
        parentURL = pathToFileURL(path.join(process.cwd(), 'repl')).href;
      } catch {
        // Continue regardless of error.
      }
      const cascadedLoader =
        require('internal/modules/esm/loader').getOrInitializeCascadedLoader();
      return cascadedLoader.import(
        specifier,
        parentURL,
        importAttributes ?? { __proto__: null },
        cascadedLoader.kEvaluationPhase,
      );
    };

    ObjectDefineProperty(context, kDynamicImportName, {
      __proto__: null,
      value: dynamicImport,
      enumerable: false,
      writable: false,
      configurable: false,
    });
  }

  async function inspectorEval(rawCode, context, file, cb) {
    installDynamicImport(context);
    prepareContext(context);

    // Wrap bare object literals so `{ a: 1 }` is evaluated as an expression
    // rather than a block statement.
    let code = rawCode;
    if (isObjectLiteral(code) && isValidSyntax(code)) {
      code = `(${StringPrototypeTrim(rawCode)})\n`;
    }

    // Rewrite ES module syntax into runnable script form.
    code = transformModuleSyntax(code).code;

    // Strict mode opt-in keeps parity with the classic REPL. `void 0` prevents
    // the directive itself from becoming the completion value.
    if (
      repl.replMode === REPL_MODE_STRICT &&
      RegExpPrototypeExec(blankRegExp, code) === null
    ) {
      code = `'use strict'; void 0;\n${code}`;
    }

    if (code === '\n') {
      return cb(await null);
    }

    const contextId = repl.useGlobal ? undefined : repl[kContextId];

    const evaluateParams = {
      expression: `${code}\n//# sourceURL=${file}`,
      objectGroup: inspector.objectGroup,
      silent: false,
      returnByValue: false,
      generatePreview: false,
      // Await in a second inspector command after retaining the promise's
      // objectId. See ReplInspectorChannel.evaluate().
      awaitPromise: false,
      replMode: true,
      throwOnSideEffect: false,
      ...(contextId !== undefined ? { contextId } : null),
    };

    let response;
    try {
      response = await inspector.evaluate(evaluateParams, repl.breakEvalOnSigint);
    } catch (err) {
      // Either an interruption (ERR_SCRIPT_EXECUTION_INTERRUPTED) or a
      // transport-level failure (e.g. the inspector went away). Surface it
      // rather than pretending the evaluation succeeded.
      inspector.releaseObjects();
      return err ? cb(err) : cb(kThrownValue, err);
    }

    const { result, exceptionDetails } = response;

    if (exceptionDetails) {
      let err;
      try {
        ({ value: err } = await inspector.toLiveValue(
          exceptionDetails.exception ?? result, context));
      } catch {
        err = new Recoverable(exceptionDetails.text);
      }
      inspector.releaseObjects();

      const className =
        exceptionDetails.exception?.className ?? result?.className;
      // Distinguish incomplete input (recoverable -> continuation prompt) from a
      // genuine error. `isRecoverableError` re-parses the *raw* source, so a
      // user `throw new SyntaxError(...)` is correctly treated as fatal.
      if (className === 'SyntaxError' && isRecoverableError(err, rawCode)) {
        return cb(new Recoverable(err));
      }
      if (err != null && typeof err === 'object') {
        trimInternalFrames(err);
      }
      return err ? cb(err) : cb(kThrownValue, err);
    }

    let value;
    try {
      ({ value } = await inspector.toLiveValue(result, context));
    } catch (err) {
      inspector.releaseObjects();
      return cb(err);
    }
    inspector.releaseObjects();
    return cb(null, value);
  }

  // Wrap so a single rejected promise inside the async pipeline can never leave
  // `cb` uncalled. `called`/`once` also guarantee the server's callback runs at
  // most once, preserving its `arguments.length` contract verbatim.
  return function evaluate(code, context, file, cb) {
    let called = false;
    function once() {
      if (called) return undefined;
      called = true;
      return ReflectApply(cb, this, arguments);
    }
    PromisePrototypeThen(
      inspectorEval(code, context, file, once),
      undefined,
      (err) => {
        debug('unexpected eval pipeline failure', err);
        once(err);
      },
    );
  };
}

module.exports = {
  createReplEval,
  Recoverable,
  kThrownValue,
};
