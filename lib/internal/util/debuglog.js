'use strict';

const { format } = require('internal/util/inspect');

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

function debuglog(set) {
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (debugEnvRegex.test(set)) {
      const pid = process.pid;
      emitWarningIfNeeded(set);
      debugs[set] = function debug(...args) {
        const msg = format(...args);
        console.error('%s %d: %s', set, pid, msg);
      };
    } else {
      debugs[set] = function debug() {};
    }
  }
  return debugs[set];
}

module.exports = {
  debuglog,
  initializeDebugEnv
};
