// Flags: --expose-gc
'use strict';

const common = require('../common');
const { onGC } = require('../common/gc');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { freeParser, parsers, HTTPParser } = require('_http_common');

let storeGCed = false;

const als = new AsyncLocalStorage();

function test() {
  const store = {};
  onGC(store, { ongc: common.mustCall(() => { storeGCed = true; }) });
  let parser;
  als.run(store, common.mustCall(() => {
    parser = parsers.alloc();
    parser.initialize(HTTPParser.RESPONSE, {});
  }));
  freeParser(parser);
}

test();
global.gc();
setImmediate(common.mustCall(() => {
  assert.ok(storeGCed);
}));
