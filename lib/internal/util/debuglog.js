'use strict';

const {
  FunctionPrototypeBind,
  ObjectCreate,
  ObjectDefineProperty,
  RegExp,
  RegExpPrototypeTest,
  SafeArrayIterator,
  StringPrototypeToLowerCase,
  StringPrototypeToUpperCase,
} = primordials;

const { inspect, format, formatWithOptions } = require('internal/util/inspect');

// `debugs` is deliberately initialized to undefined so any call to
// debuglog() before initializeDebugEnv() is called will throw.
let debugImpls;

let debugEnvRegex = /^$/;
let testEnabled;

// `debugEnv` is initial value of process.env.NODE_DEBUG
function initializeDebugEnv(debugEnv) {
  debugImpls = ObjectCreate(null);
  if (debugEnv) {
    debugEnv = debugEnv.replace(/[|\\{}()[\]^$+?.]/g, '\\$&')
      .replace(/\*/g, '.*')
      .replace(/,/g, '$|^')
      .toUpperCase();
    debugEnvRegex = new RegExp(`^${debugEnv}$`, 'i');
  }
  testEnabled = FunctionPrototypeBind(RegExpPrototypeTest, null, debugEnvRegex);
}

// Emits warning when user sets
// NODE_DEBUG=http or NODE_DEBUG=http2.
function emitWarningIfNeeded(set) {
  if ('HTTP' === set || 'HTTP2' === set) {
    process.emitWarning('Setting the NODE_DEBUG environment variable ' +
      'to \'' + StringPrototypeToLowerCase(set) + '\' can expose sensitive ' +
      'data (such as passwords, tokens and authentication headers) ' +
      'in the resulting log.');
  }
}

const noop = () => {};

function debuglogImpl(enabled, set) {
  if (debugImpls[set] === undefined) {
    if (enabled) {
      const pid = process.pid;
      emitWarningIfNeeded(set);
      debugImpls[set] = function debug(...args) {
        const colors = process.stderr.hasColors && process.stderr.hasColors();
        const msg = formatWithOptions({ colors }, ...args);
        const coloredPID = inspect(pid, { colors });
        process.stderr.write(format('%s %s: %s\n', set, coloredPID, msg));
      };
    } else {
      debugImpls[set] = noop;
    }
  }
  return debugImpls[set];
}

// debuglogImpl depends on process.pid and process.env.NODE_DEBUG,
// so it needs to be called lazily in top scopes of internal modules
// that may be loaded before these run time states are allowed to
// be accessed.
function debuglog(set, cb) {
  function init() {
    set = StringPrototypeToUpperCase(set);
    enabled = testEnabled(set);
  }
  let debug = (...args) => {
    init();
    // Only invokes debuglogImpl() when the debug function is
    // called for the first time.
    debug = debuglogImpl(enabled, set);
    if (typeof cb === 'function')
      cb(debug);
    switch (args.length) {
      case 0: return debug();
      case 1: return debug(args[0]);
      case 2: return debug(args[0], args[1]);
      default: return debug(...new SafeArrayIterator(args));
    }
  };
  let enabled;
  let test = () => {
    init();
    test = () => enabled;
    return enabled;
  };
  const logger = (...args) => {
    switch (args.length) {
      case 0: return debug();
      case 1: return debug(args[0]);
      case 2: return debug(args[0], args[1]);
      default: return debug(...new SafeArrayIterator(args));
    }
  };
  ObjectDefineProperty(logger, 'enabled', {
    get() {
      return test();
    },
    configurable: true,
    enumerable: true
  });
  return logger;
}

module.exports = {
  debuglog,
  initializeDebugEnv
};
