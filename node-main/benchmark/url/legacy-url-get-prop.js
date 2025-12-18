'use strict';
const common = require('../common.js');
const url = require('url');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: common.urlDataTypes,
  e: [1],
});

function main({ type, e }) {
  const data = common.bakeUrlData(type, e, false, false).map((i) => url.parse(i));
  const obj = url.parse(data[0]);
  const noDead = {
    protocol: obj.protocol,
    auth: obj.auth,
    host: obj.host,
    hostname: obj.hostname,
    port: obj.port,
    pathname: obj.pathname,
    search: obj.search,
    hash: obj.hash,
  };
  const len = data.length;
  // It's necessary to assign the values to an object
  // to avoid loop invariant code motion.
  bench.start();
  for (let i = 0; i < len; i++) {
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
  assert.ok(noDead);
}
