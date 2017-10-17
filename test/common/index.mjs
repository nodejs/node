// Flags: --experimental-modules
/* eslint-disable required-modules */
'use strict';

let knownGlobals = [
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

knownGlobals.push(global.gc);

knownGlobals.push(DTRACE_HTTP_SERVER_RESPONSE);
knownGlobals.push(DTRACE_HTTP_SERVER_REQUEST);
knownGlobals.push(DTRACE_HTTP_CLIENT_RESPONSE);
knownGlobals.push(DTRACE_HTTP_CLIENT_REQUEST);
knownGlobals.push(DTRACE_NET_STREAM_END);
knownGlobals.push(DTRACE_NET_SERVER_CONNECTION);

knownGlobals.push(COUNTER_NET_SERVER_CONNECTION);
knownGlobals.push(COUNTER_NET_SERVER_CONNECTION_CLOSE);
knownGlobals.push(COUNTER_HTTP_SERVER_REQUEST);
knownGlobals.push(COUNTER_HTTP_SERVER_RESPONSE);
knownGlobals.push(COUNTER_HTTP_CLIENT_REQUEST);
knownGlobals.push(COUNTER_HTTP_CLIENT_RESPONSE);

knownGlobals.push(LTTNG_HTTP_SERVER_RESPONSE);
knownGlobals.push(LTTNG_HTTP_SERVER_REQUEST);
knownGlobals.push(LTTNG_HTTP_CLIENT_RESPONSE);
knownGlobals.push(LTTNG_HTTP_CLIENT_REQUEST);
knownGlobals.push(LTTNG_NET_STREAM_END);
knownGlobals.push(LTTNG_NET_SERVER_CONNECTION);

knownGlobals.push(ArrayBuffer);
knownGlobals.push(Int8Array);
knownGlobals.push(Uint8Array);
knownGlobals.push(Uint8ClampedArray);
knownGlobals.push(Int16Array);
knownGlobals.push(Uint16Array);
knownGlobals.push(Int32Array);
knownGlobals.push(Uint32Array);
knownGlobals.push(Float32Array);
knownGlobals.push(Float64Array);
knownGlobals.push(DataView);

knownGlobals.push(Proxy);

knownGlobals.push(Symbol);

if (process.env.NODE_TEST_KNOWN_GLOBALS) {
  const knownFromEnv = process.env.NODE_TEST_KNOWN_GLOBALS.split(',');
  allowGlobals(...knownFromEnv);
}

export function allowGlobals(...whitelist) {
  knownGlobals = knownGlobals.concat(whitelist);
}

export function leakedGlobals() {
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
export var globalCheck = true;

process.on('exit', function() {
  if (!globalCheck) return;
  const leaked = leakedGlobals();
  if (leaked.length > 0) {
    assert.fail(`Unexpected global(s) found: ${leaked.join(', ')}`);
  }
});
