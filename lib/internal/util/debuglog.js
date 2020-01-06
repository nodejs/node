'use strict';

const {
  RegExp,
} = primordials;

const { inspect, format, formatWithOptions } = require('internal/util/inspect');

// `debugs` is deliberately initialized to undefined so any call to
// debuglog() before initializeDebugEnv() is called will throw.
let debugs;

let debugEnvRegex = /^$/;

// `debugEnv` is initial value of process.env.NODE_DEBUG
function initializeDebugEnv(debugEnv) {
  debugs = {};
  if (debugEnv) {
    debugEnv = debugEnv.replace(/[|\\{}()[\]^$+?.]/g, '\\$&')
      .replace(/\*/g, '.*')
      .replace(/,/g, '$|^')
      .toUpperCase();
    debugEnvRegex = new RegExp(`^${debugEnv}$`, 'i');
  }
}

// Emits warning when user sets
// NODE_DEBUG=http or NODE_DEBUG=http2.
function emitWarningIfNeeded(set) {
  if ('HTTP' === set || 'HTTP2' === set) {
    process.emitWarning('Setting the NODE_DEBUG environment variable ' +
      'to \'' + set.toLowerCase() + '\' can expose sensitive ' +
      'data (such as passwords, tokens and authentication headers) ' +
      'in the resulting log.');
  }
}

function debuglogImpl(set) {
  set = set.toUpperCase();
  if (debugs[set] === undefined) {
    if (debugEnvRegex.test(set)) {
      const pid = process.pid;
      emitWarningIfNeeded(set);
      debugs[set] = function debug(...args) {
        const colors = process.stderr.hasColors && process.stderr.hasColors();
        const msg = formatWithOptions({ colors }, ...args);
        const coloredPID = inspect(pid, { colors });
        process.stderr.write(format('%s %s: %s\n', set, coloredPID, msg));
      };
    } else {
      debugs[set] = null;
    }
  }
  return debugs[set];
}

// debuglogImpl depends on process.pid and process.env.NODE_DEBUG,
// so it needs to be called lazily in top scopes of internal modules
// that may be loaded before these run time states are allowed to
// be accessed.
function debuglog(set) {
  let debug;
  return function(...args) {
    if (debug === undefined) {
      // Only invokes debuglogImpl() when the debug function is
      // called for the first time.
      debug = debuglogImpl(set);
    }
    if (debug !== null)
      debug(...args);
  };
}

module.exports = {
  debuglog,
  initializeDebugEnv
};
