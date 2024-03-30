'use strict';

const {
  ObjectDefineProperty,
  RegExp,
  RegExpPrototypeExec,
  SafeArrayIterator,
  SafeMap,
  StringPrototypeToLowerCase,
  StringPrototypeToUpperCase,
} = primordials;

const { inspect, format, formatWithOptions } = require('internal/util/inspect');
const { isTraceCategoryEnabled } = internalBinding('trace_events');

let _traceTimer;
function lazyTraceTimer() {
  if (_traceTimer) {
    return _traceTimer;
  }

  _traceTimer = require('internal/util/trace_timer');
  return _traceTimer;
}

// `debugImpls` and `testEnabled` are deliberately not initialized so any call
// to `debuglog()` before `initializeDebugEnv()` is called will throw.
let debugImpls;
let testEnabled;


// `debugEnv` is initial value of process.env.NODE_DEBUG
function initializeDebugEnv(debugEnv) {
  debugImpls = { __proto__: null };
  if (debugEnv) {
    // This is run before any user code, it's OK not to use primordials.
    debugEnv = debugEnv.replace(/[|\\{}()[\]^$+?.]/g, '\\$&')
      .replaceAll('*', '.*')
      .replaceAll(',', '$|^');
    const debugEnvRegex = new RegExp(`^${debugEnv}$`, 'i');
    testEnabled = (str) => RegExpPrototypeExec(debugEnvRegex, str) !== null;
  } else {
    testEnabled = () => false;
  }
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

const noop = () => { };

let utilColors;
function lazyUtilColors() {
  utilColors ??= require('internal/util/colors');
  return utilColors;
}

/**
 * @typedef {(enabled: boolean, set: string) => any} DebugLogImplementation
 */

/**
 * @type {DebugLogImplementation}
 */
function debuglogImpl(enabled, set, args) {
  if (debugImpls[set] === undefined) {
    if (enabled) {
      const pid = process.pid;
      emitWarningIfNeeded(set);
      debugImpls[set] = function debug(...args) {
        const colors = lazyUtilColors().shouldColorize(process.stderr);
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

/**
 * @typedef {(fn: DebugLogFn) => void} DebugLogCallback
 */

/**
 * @typedef {(message: string, ...args: any[]) => void} DebugLogFn
 */

// debuglogImpl depends on process.pid and process.env.NODE_DEBUG,
// so it needs to be called lazily in top scopes of internal modules
// that may be loaded before these run time states are allowed to
// be accessed.
/**
 * @param {string} set
 * @param {DebugLogCallback} cb
 * @returns {DebugLogFn}
 */
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
      case 1: return debug(args[0]);
      case 2: return debug(args[0], args[1]);
      default: return debug(...new SafeArrayIterator(args));
    }
  };
  ObjectDefineProperty(logger, 'enabled', {
    __proto__: null,
    get() {
      return test();
    },
    configurable: true,
    enumerable: true,
  });
  return logger;
}

function buildCategory(set) {
  return `node,node.debuglog_${set}`;
}

/**
 * @type {Record<string, SafeMap>}
 */
let tracesStores;

/**
 * @typedef {(logLabel: string, traceLabel?: string) => void} TimerStart
 */

/**
 * @typedef {(label: string, traceLabel?: string) => void} TimerEnd
 */

/**
 * @typedef {(label: string, traceLabel?: string, args?: any[]) => void} TimerLog
 */

/**
 * Debuglog with time fns and support for trace
 * @param {string} set
 * @param {(startTimer: TimerStart, endTimer: TimerEnd, logTimer: TimerLog) => void} cb
 * @returns {{startTimer: TimerStart, endTimer: TimerEnd, logTimer: TimerLog}}
 */
function debugWithTimer(set, cb) {
  set = StringPrototypeToUpperCase(set);

  if (tracesStores === undefined) {
    tracesStores = { __proto__: null };
  }

  /**
   * @type {import('internal/util/trace_timer').LogImpl}
   */
  function logImpl(label, timeFormatted, args) {
    const pid = process.pid;
    const colors = { colors: lazyUtilColors().shouldColorize(process.stderr) };
    const coloredPID = inspect(pid, colors);

    if (args === undefined)
      process.stderr.write(format('%s %s %s: %s\n', set, coloredPID, label, timeFormatted));
    else
      process.stderr.write(
        format(
          '%s %s %s: %s\n',
          set,
          coloredPID,
          label,
          timeFormatted,
          ...new SafeArrayIterator(args),
        ),
      );
  }

  const kTraceCategory = buildCategory(StringPrototypeToLowerCase(set));
  /**
   * @type {import('internal/util/trace_timer').LogImpl}
   */
  let selectedLogImpl;
  let categoryEnabled = false;

  /**
   * @type {TimerStart}
   */
  function internalStartTimer(logLabel, traceLabel) {
    if (!categoryEnabled && !isTraceCategoryEnabled(kTraceCategory)) {
      return;
    }

    lazyTraceTimer().time(tracesStores[set], kTraceCategory, 'debuglog.time', logLabel, traceLabel);
  }

  /**
   * @type {TimerEnd}
   */
  function internalEndTimer(logLabel, traceLabel) {
    if (!categoryEnabled && !isTraceCategoryEnabled(kTraceCategory)) {
      return;
    }

    lazyTraceTimer()
      .timeEnd(tracesStores[set], kTraceCategory, 'debuglog.timeEnd', selectedLogImpl, logLabel, traceLabel);
  }

  /**
   * @type {TimerLog}
   */
  function internalLogTimer(logLabel, traceLabel, args) {
    if (!categoryEnabled && !isTraceCategoryEnabled(kTraceCategory)) {
      return;
    }

    lazyTraceTimer()
      .timeLog(tracesStores[set], kTraceCategory, 'debuglog.timeLog', selectedLogImpl, logLabel, traceLabel, args);
  }

  function init() {
    if (tracesStores[set] === undefined) {
      tracesStores[set] = new SafeMap();
    }
    emitWarningIfNeeded(set);
    categoryEnabled = testEnabled(set);
    selectedLogImpl = categoryEnabled ? logImpl : noop;
  }

  /**
   * @type {TimerStart}
   */
  const startTimer = (logLabel, traceLabel) => {
    init();

    if (cb)
      cb(internalStartTimer, internalEndTimer, internalLogTimer);

    return internalStartTimer(logLabel, traceLabel);
  };

  /**
   * @type {TimerEnd}
   */
  const endTimer = (logLabel, traceLabel) => {
    init();

    if (cb)
      cb(internalStartTimer, internalEndTimer, internalLogTimer);

    return internalEndTimer(logLabel, traceLabel);
  };

  /**
   * @type {TimerLog}
   */
  const logTimer = (logLabel, traceLabel, args) => {
    init();

    if (cb)
      cb(internalStartTimer, internalEndTimer, internalLogTimer);

    return internalLogTimer(logLabel, traceLabel, args);
  };

  return {
    startTimer,
    endTimer,
    logTimer,
  };
}

module.exports = {
  debuglog,
  debugWithTimer,
  initializeDebugEnv,
};
