'use strict';
const common = require('../common.js');
const url = require('url');
const URL = url.URL;
const assert = require('assert');

const bench = common.createBenchmark(main, {
  type: common.urlDataTypes,
  e: [1],
});

function main({ type, e }) {
  const data = common.bakeUrlData(type, e, false, true);
  const obj = new URL(data[0]);
  const noDead = {
    protocol: obj.protocol,
    auth: `${obj.username}:${obj.password}`,
    host: obj.host,
    hostname: obj.hostname,
    port: obj.port,
    pathname: obj.pathname,
    search: obj.search,
    hash: obj.hash,
  };
  const len = data.length;
  bench.start();
  for (let i = 0; i < len; i++) {
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
  assert.ok(noDead);
}
