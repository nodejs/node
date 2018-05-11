// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */

import assert from 'assert';

let knownGlobals = [
  ArrayBuffer,
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  DataView,
  Proxy,
  Symbol,
  Buffer,
  clearImmediate,
  clearInterval,
  clearTimeout,
  console,
  constructor, // Enumerable in V8 3.21.
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

// Turn this off if the test should not check for global leaks.
export let globalCheck = true;  // eslint-disable-line

process.on('exit', function() {
  if (!globalCheck) return;
  const leaked = leakedGlobals();
  if (leaked.length > 0) {
    assert.fail(`Unexpected global(s) found: ${leaked.join(', ')}`);
  }
});
