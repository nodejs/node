'use strict';

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

function evalScript(name, body, breakFirstLine) {
  const CJSModule = require('internal/modules/cjs/loader');
  const { kVmBreakFirstLineSymbol } = require('internal/util');

  const cwd = tryGetCwd();

  const module = new CJSModule(name);
  module.filename = path.join(cwd, name);
  module.paths = CJSModule._nodeModulePaths(cwd);
  global.kVmBreakFirstLineSymbol = kVmBreakFirstLineSymbol;
  const script = `
    global.__filename = ${JSON.stringify(name)};
    global.exports = exports;
    global.module = module;
    global.__dirname = __dirname;
    global.require = require;
    const { kVmBreakFirstLineSymbol } = global;
    delete global.kVmBreakFirstLineSymbol;
    return require("vm").runInThisContext(
      ${JSON.stringify(body)}, {
        filename: ${JSON.stringify(name)},
        displayErrors: true,
        [kVmBreakFirstLineSymbol]: ${!!breakFirstLine}
      });\n`;
  const result = module._compile(script, `${name}-wrapper`);
  if (require('internal/options').getOptionValue('--print')) {
    console.log(result);
  }
  // Handle any nextTicks added in the first tick of the program.
  process._tickCallback();
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
// this may indicate that node::FatalException should fix up the callback scope
// before calling into process._fatalException, or this function should
// take extra care of the async hooks before it schedules a setImmediate.
function createFatalException() {
  return (er) => {
    // It's possible that defaultTriggerAsyncId was set for a constructor
    // call that threw and was never cleared. So clear it now.
    clearDefaultTriggerAsyncId();

    // If node-report is enabled, call into its handler to see
    // whether it is interested in handling the situation.
    // Ignore if the error is scoped inside a domain.
    // use == in the checks as we want to allow for null and undefined
    if (er == null || er.domain == null) {
      try {
        const report = internalBinding('report');
        if (report != null &&
            require('internal/options')
              .getOptionValue('--experimental-report')) {
          const config = {};
          report.syncConfig(config, false);
          if (Array.isArray(config.events) &&
              config.events.includes('exception')) {
            report.onUnCaughtException(er ? er.stack : undefined);
          }
        }
      } catch {}  // NOOP, node_report unavailable.
    }

    if (exceptionHandlerState.captureFn !== null) {
      exceptionHandlerState.captureFn(er);
    } else if (!process.emit('uncaughtException', er)) {
      // If someone handled it, then great.  otherwise, die in C++ land
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
      try {
        const { kExpandStackSymbol } = require('internal/util');
        if (typeof er[kExpandStackSymbol] === 'function')
          er[kExpandStackSymbol]();
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
  evalScript,
  fatalException: createFatalException(),
  setUncaughtExceptionCaptureCallback,
  hasUncaughtExceptionCaptureCallback
};
