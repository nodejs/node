'use strict';

const sleep = require('util').promisify(setTimeout);
const assert = require('assert');
const common = require('../common');
const { currentResource, createHook } = require('async_hooks');
const { createServer, get } = require('http');
const sym = Symbol('cls');
const id = Symbol('id');

// tests continuation local storage with the currentResource API
// through an async function

createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    const cr = currentResource();
    resource[id] = asyncId;
    if (cr) {
      resource[sym] = cr[sym];
    }
  }
}).enable();

async function handler(req, res) {
  currentResource()[sym] = { state: req.url };
  await sleep(10);
  const { state } = currentResource()[sym];
  res.setHeader('content-type', 'application/json');
  res.end(JSON.stringify({ state }));
}

const server = createServer(function(req, res) {
  handler(req, res);
});

function test(n) {
  get(`http://localhost:${server.address().port}/${n}`, common.mustCall(function(res) {
    res.setEncoding('utf8');

    let body = '';
    res.on('data', function(chunk) {
      body += chunk;
    });

    res.on('end', common.mustCall(function() {
      assert.deepStrictEqual(JSON.parse(body), { state: `/${n}` });
    }));
  }));
}

server.listen(0, common.mustCall(function() {
  server.unref();
  for (let i = 0; i < 10; i++) {
    test(i);
  }
}));
