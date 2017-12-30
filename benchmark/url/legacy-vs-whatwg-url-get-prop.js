'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');
const inputs = require('../fixtures/url-inputs.js').urls;

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  method: ['legacy', 'whatwg'],
  n: [1e5]
});

// At the time of writing, when using a passed property name to index
// the object, Crankshaft would generate a LoadKeyedGeneric even when it
// remains a constant in the function, so here we must use the literal
// instead to get a LoadNamedField.
function useLegacy(n, input) {
  const obj = url.parse(input);
  const noDead = {
    protocol: obj.protocol,
    auth: obj.auth,
    host: obj.host,
    hostname: obj.hostname,
    port: obj.port,
    pathname: obj.pathname,
    search: obj.search,
    hash: obj.hash
  };
  // It's necessary to assign the values to an object
  // to avoid loop invariant code motion.
  bench.start();
  for (var i = 0; i < n; i += 1) {
    noDead.protocol = obj.protocol;
    noDead.auth = obj.auth;
    noDead.host = obj.host;
    noDead.hostname = obj.hostname;
    noDead.port = obj.port;
    noDead.pathname = obj.pathname;
    noDead.search = obj.search;
    noDead.hash = obj.hash;
  }
  bench.end(n);
  return noDead;
}

function useWHATWG(n, input) {
  const obj = new URL(input);
  const noDead = {
    protocol: obj.protocol,
    auth: `${obj.username}:${obj.password}`,
    host: obj.host,
    hostname: obj.hostname,
    port: obj.port,
    pathname: obj.pathname,
    search: obj.search,
    hash: obj.hash
  };
  bench.start();
  for (var i = 0; i < n; i += 1) {
    noDead.protocol = obj.protocol;
    noDead.auth = `${obj.username}:${obj.password}`;
    noDead.host = obj.host;
    noDead.hostname = obj.hostname;
    noDead.port = obj.port;
    noDead.pathname = obj.pathname;
    noDead.search = obj.search;
    noDead.hash = obj.hash;
  }
  bench.end(n);
  return noDead;
}

function main({ type, n, method }) {
  const input = inputs[type];
  if (!input) {
    throw new Error('Unknown input type');
  }

  var noDead;  // Avoid dead code elimination.
  switch (method) {
    case 'legacy':
      noDead = useLegacy(n, input);
      break;
    case 'whatwg':
      noDead = useWHATWG(n, input);
      break;
    default:
      throw new Error('Unknown method');
  }

  assert.ok(noDead);
}
