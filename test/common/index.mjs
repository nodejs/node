// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */

import assert from 'assert';

let knownGlobals = [
  Buffer,
  clearImmediate,
  clearInterval,
  clearTimeout,
  global,
  process,
  setImmediate,
  setInterval,
  setTimeout
];

if (process.env.NODE_TEST_KNOWN_GLOBALS) {
  const knownFromEnv = process.env.NODE_TEST_KNOWN_GLOBALS.split(',');
  allowGlobals(...knownFromEnv);
}

export function allowGlobals(...whitelist) {
  knownGlobals = knownGlobals.concat(whitelist);
}

export function leakedGlobals() {
  // Add possible expected globals
  if (global.gc) {
    knownGlobals.push(global.gc);
  }

  if (global.DTRACE_HTTP_SERVER_RESPONSE) {
    knownGlobals.push(DTRACE_HTTP_SERVER_RESPONSE);
    knownGlobals.push(DTRACE_HTTP_SERVER_REQUEST);
    knownGlobals.push(DTRACE_HTTP_CLIENT_RESPONSE);
    knownGlobals.push(DTRACE_HTTP_CLIENT_REQUEST);
    knownGlobals.push(DTRACE_NET_STREAM_END);
    knownGlobals.push(DTRACE_NET_SERVER_CONNECTION);
  }

  if (global.COUNTER_NET_SERVER_CONNECTION) {
    knownGlobals.push(COUNTER_NET_SERVER_CONNECTION);
    knownGlobals.push(COUNTER_NET_SERVER_CONNECTION_CLOSE);
    knownGlobals.push(COUNTER_HTTP_SERVER_REQUEST);
    knownGlobals.push(COUNTER_HTTP_SERVER_RESPONSE);
    knownGlobals.push(COUNTER_HTTP_CLIENT_REQUEST);
    knownGlobals.push(COUNTER_HTTP_CLIENT_RESPONSE);
  }

  const leaked = [];

  for (const val in global) {
    if (!knownGlobals.includes(global[val])) {
      leaked.push(val);
    }
  }

  if (global.__coverage__) {
    return leaked.filter((varname) => !/^(?:cov_|__cov)/.test(varname));
  } else {
    return leaked;
  }
}

process.on('exit', function() {
  const leaked = leakedGlobals();
  if (leaked.length > 0) {
    assert.fail(`Unexpected global(s) found: ${leaked.join(', ')}`);
  }
});
