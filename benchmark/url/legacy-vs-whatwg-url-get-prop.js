'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: common.urlDataTypes,
  method: ['legacy', 'whatwg'],
  e: [1]
});

function useLegacy(data) {
  const obj = url.parse(data[0]);
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
  const len = data.length;
  // It's necessary to assign the values to an object
  // to avoid loop invariant code motion.
  bench.start();
  for (var i = 0; i < len; i++) {
    const obj = data[i];
    noDead.protocol = obj.protocol;
    noDead.auth = obj.auth;
    noDead.host = obj.host;
    noDead.hostname = obj.hostname;
    noDead.port = obj.port;
    noDead.pathname = obj.pathname;
    noDead.search = obj.search;
    noDead.hash = obj.hash;
  }
  bench.end(len);
  return noDead;
}

function useWHATWG(data) {
  const obj = new URL(data[0]);
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
  const len = data.length;
  bench.start();
  for (var i = 0; i < len; i++) {
    const obj = data[i];
    noDead.protocol = obj.protocol;
    noDead.auth = `${obj.username}:${obj.password}`;
    noDead.host = obj.host;
    noDead.hostname = obj.hostname;
    noDead.port = obj.port;
    noDead.pathname = obj.pathname;
    noDead.search = obj.search;
    noDead.hash = obj.hash;
  }
  bench.end(len);
  return noDead;
}

function main({ type, method, e }) {
  e = +e;
  var data;
  var noDead;  // Avoid dead code elimination.
  switch (method) {
    case 'legacy':
      data = common.bakeUrlData(type, e, false, false);
      noDead = useLegacy(data.map((i) => url.parse(i)));
      break;
    case 'whatwg':
      data = common.bakeUrlData(type, e, false, true);
      noDead = useWHATWG(data);
      break;
    default:
      throw new Error(`Unknown method "${method}"`);
  }

  assert.ok(noDead);
}
