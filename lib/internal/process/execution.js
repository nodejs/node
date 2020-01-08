'use strict';

const {
  JSONStringify,
  PromiseResolve,
} = primordials;

const path = require('path');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_UNCAUGHT_EXCEPTION_CAPTURE_ALREADY_SET
  }
} = require('internal/errors');

const {
  executionAsyncId,
  clearDefaultTriggerAsyncId,
  clearAsyncIdStack,
  hasAsyncIdStack,
  afterHooksExist,
  emitAfter
} = require('internal/async_hooks');

// shouldAbortOnUncaughtToggle is a typed array for faster
// communication with JS.
const { shouldAbortOnUncaughtToggle } = internalBinding('util');

function tryGetCwd() {
  try {
    return process.cwd();
  } catch {
    // getcwd(3) can fail if the current working directory has been deleted.
    // Fall back to the directory name of the (absolute) executable path.
    // It's not really correct but what are the alternatives?
    return path.dirname(process.execPath);
  }
}

function evalModule(source, print) {
  const { log, error } = require('internal/console/global');
  const { decorateErrorStack } = require('internal/util');
  const asyncESM = require('internal/process/esm_loader');
  PromiseResolve(asyncESM.ESMLoader).then(async (loader) => {
    const { result } = await loader.eval(source);
    if (print) {
      log(result);
    }
  })
  .catch((e) => {
    decorateErrorStack(e);
    error(e);
    process.exit(1);
  });
}

function evalScript(name, body, breakFirstLine, print) {
  const CJSModule = require('internal/modules/cjs/loader').Module;
  const { kVmBreakFirstLineSymbol } = require('internal/util');
  const { pathToFileURL } = require('url');

  const cwd = tryGetCwd();
  const origModule = global.module;  // Set e.g. when called from the REPL.

  const module = new CJSModule(name);
  module.filename = path.join(cwd, name);
  module.paths = CJSModule._nodeModulePaths(cwd);

  global.kVmBreakFirstLineSymbol = kVmBreakFirstLineSymbol;
  global.asyncESM = require('internal/process/esm_loader');

  const baseUrl = pathToFileURL(module.filename).href;

  const script = `
    global.__filename = ${JSONStringify(name)};
    global.exports = exports;
    global.module = module;
    global.__dirname = __dirname;
    global.require = require;
    const { kVmBreakFirstLineSymbol, asyncESM } = global;
    delete global.kVmBreakFirstLineSymbol;
    delete global.asyncESM;
    return require("vm").runInThisContext(
      ${JSONStringify(body)}, {
        filename: ${JSONStringify(name)},
        displayErrors: true,
        [kVmBreakFirstLineSymbol]: ${!!breakFirstLine},
        async importModuleDynamically (specifier) {
          const loader = await asyncESM.ESMLoader;
          return loader.import(specifier, ${JSONStringify(baseUrl)});
        }
      });\n`;
  const result = module._compile(script, `${name}-wrapper`);
  if (print) {
    const { log } = require('internal/console/global');
    log(result);
  }

  if (origModule !== undefined)
    global.module = origModule;
}

const exceptionHandlerState = { captureFn: null };

function setUncaughtExceptionCaptureCallback(fn) {
  if (fn === null) {
    exceptionHandlerState.captureFn = fn;
    shouldAbortOnUncaughtToggle[0] = 1;
    return;
  }
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('fn', ['Function', 'null'], fn);
  }
  if (exceptionHandlerState.captureFn !== null) {
    throw new ERR_UNCAUGHT_EXCEPTION_CAPTURE_ALREADY_SET();
  }
  exceptionHandlerState.captureFn = fn;
  shouldAbortOnUncaughtToggle[0] = 0;
}

function hasUncaughtExceptionCaptureCallback() {
  return exceptionHandlerState.captureFn !== null;
}

function noop() {}

// XXX(joyeecheung): for some reason this cannot be defined at the top-level
// and exported to be written to process._fatalException, it has to be
// returned as an *anonymous function* wrapped inside a factory function,
// otherwise it breaks the test-timers.setInterval async hooks test -
// this may indicate that node::errors::TriggerUncaughtException() should
// fix up the callback scope before calling into process._fatalException,
// or this function should take extra care of the async hooks before it
// schedules a setImmediate.
function createOnGlobalUncaughtException() {
  // The C++ land node::errors::TriggerUncaughtException() will
  // exit the process if it returns false, and continue execution if it
  // returns true (which indicates that the exception is handled by the user).
  return (er, fromPromise) => {
    // It's possible that defaultTriggerAsyncId was set for a constructor
    // call that threw and was never cleared. So clear it now.
    clearDefaultTriggerAsyncId();

    // If diagnostic reporting is enabled, call into its handler to see
    // whether it is interested in handling the situation.
    // Ignore if the error is scoped inside a domain.
    // use == in the checks as we want to allow for null and undefined
    if (er == null || er.domain == null) {
      try {
        const report = internalBinding('report');
        if (report != null && report.shouldReportOnUncaughtException()) {
          report.writeReport(er ? er.message : 'Exception',
                             'Exception',
                             null,
                             er ? er.stack : undefined);
        }
      } catch {}  // Ignore the exception. Diagnostic reporting is unavailable.
    }

    const type = fromPromise ? 'unhandledRejection' : 'uncaughtException';
    process.emit('uncaughtExceptionMonitor', er, type);
    if (exceptionHandlerState.captureFn !== null) {
      exceptionHandlerState.captureFn(er);
    } else if (!process.emit('uncaughtException', er, type)) {
      // If someone handled it, then great. Otherwise, die in C++ land
      // since that means that we'll exit the process, emit the 'exit' event.
      try {
        if (!process._exiting) {
          process._exiting = true;
          process.exitCode = 1;
          process.emit('exit', 1);
        }
      } catch {
        // Nothing to be done about it at this point.
      }
      return false;
    }

    // If we handled an error, then make sure any ticks get processed
    // by ensuring that the next Immediate cycle isn't empty.
    require('timers').setImmediate(noop);

    // Emit the after() hooks now that the exception has been handled.
    if (afterHooksExist()) {
      do {
        emitAfter(executionAsyncId());
      } while (hasAsyncIdStack());
    // Or completely empty the id stack.
    } else {
      clearAsyncIdStack();
    }

    return true;
  };
}

function readStdin(callback) {
  process.stdin.setEncoding('utf8');

  let code = '';
  process.stdin.on('data', (d) => {
    code += d;
  });

  process.stdin.on('end', () => {
    callback(code);
  });
}

module.exports = {
  readStdin,
  tryGetCwd,
  evalModule,
  evalScript,
  onGlobalUncaughtException: createOnGlobalUncaughtException(),
  setUncaughtExceptionCaptureCallback,
  hasUncaughtExceptionCaptureCallback
};
