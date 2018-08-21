'use strict';

const { promisify } = require('util');
const { readFile } = require('fs');
const sleep = promisify(setTimeout);
const read = promisify(readFile);
const common = require('../common.js');
const {
  createHook,
  currentResource,
  executionAsyncId
} = require('async_hooks');
const { createServer } = require('http');

// configuration for the http server
// there is no need for parameters in this test
const connections = 500;
const path = '/';

const bench = common.createBenchmark(main, {
  type: ['current-resource', 'destroy'],
  method: ['callbacks', 'async'],
  n: [1e6]
});

function buildCurrentResource(getServe) {
  const server = createServer(getServe(getCLS, setCLS));
  const hook = createHook({ init });
  const cls = Symbol('cls');
  let closed = false;
  hook.enable();

  return {
    server,
    close
  };

  function getCLS() {
    // we need to protect this, as once the hook is
    // disabled currentResource will return null
    if (closed) {
      return;
    }

    const resource = currentResource();
    if (!resource[cls]) {
      return null;
    }
    return resource[cls].state;
  }

  function setCLS(state) {
    // we need to protect this, as once the hook is
    // disabled currentResource will return null
    if (closed) {
      return;
    }
    const resource = currentResource();
    if (!resource[cls]) {
      resource[cls] = { state };
    } else {
      resource[cls].state = state;
    }
  }

  function init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'TIMERWRAP') return;

    var cr = currentResource();
    if (cr) {
      resource[cls] = cr[cls];
    }
  }

  function close() {
    closed = true;
    hook.disable();
    server.close();
  }
}

function buildDestroy(getServe) {
  const transactions = new Map();
  const server = createServer(getServe(getCLS, setCLS));
  const hook = createHook({ init, destroy });
  hook.enable();

  return {
    server,
    close
  };

  function getCLS() {
    const asyncId = executionAsyncId();
    return transactions.has(asyncId) ? transactions.get(asyncId) : null;
  }

  function setCLS(value) {
    const asyncId = executionAsyncId();
    transactions.set(asyncId, value);
  }

  function init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'TIMERWRAP') return;

    transactions.set(asyncId, getCLS());
  }

  function destroy(asyncId) {
    transactions.delete(asyncId);
  }

  function close() {
    hook.disable();
    server.close();
  }
}

function getServeAwait(getCLS, setCLS) {
  return async function serve(req, res) {
    setCLS(Math.random());
    await sleep(10);
    await read(__filename);
    res.setHeader('content-type', 'application/json');
    res.end(JSON.stringify({ cls: getCLS() }));
  };
}

function getServeCallbacks(getCLS, setCLS) {
  return function serve(req, res) {
    setCLS(Math.random());
    setTimeout(function() {
      readFile(__filename, function() {
        res.setHeader('content-type', 'application/json');
        res.end(JSON.stringify({ cls: getCLS() }));
      });
    }, 10);
  };
}

const types = {
  'current-resource': buildCurrentResource,
  'destroy': buildDestroy
};

const asyncMethod = {
  'callbacks': getServeCallbacks,
  'async': getServeAwait
};

function main({ type, method }) {
  const { server, close } = types[type](asyncMethod[method]);

  server
    .listen(common.PORT)
    .on('listening', function() {

      bench.http({
        path,
        connections
      }, function() {
        close();
      });
    });
}
