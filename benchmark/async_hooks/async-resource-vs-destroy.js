'use strict';

const { promisify } = require('util');
const { readFile } = require('fs');
const sleep = promisify(setTimeout);
const read = promisify(readFile);
const common = require('../common.js');
const {
  createHook,
  executionAsyncResource,
  executionAsyncId,
  AsyncLocalStorage
} = require('async_hooks');
const { createServer } = require('http');

const bench = common.createBenchmark(main, {
  type: ['async-resource', 'destroy', 'async-local-storage'],
  asyncMethod: ['callbacks', 'async'],
  path: '/',
  connections: 500,
  duration: 5,
  n: [1e6]
});

function buildCurrentResource(getServe) {
  const server = createServer(getServe(getCLS, setCLS));
  const hook = createHook({ init });
  const cls = Symbol('cls');
  hook.enable();

  return {
    server,
    close
  };

  function getCLS() {
    const resource = executionAsyncResource();
    if (!resource[cls]) {
      return null;
    }
    return resource[cls].state;
  }

  function setCLS(state) {
    const resource = executionAsyncResource();
    if (!resource[cls]) {
      resource[cls] = { state };
    } else {
      resource[cls].state = state;
    }
  }

  function init(asyncId, type, triggerAsyncId, resource) {
    const cr = executionAsyncResource();
    if (cr !== null) {
      resource[cls] = cr[cls];
    }
  }

  function close() {
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

function buildAsyncLocalStorage(getServe) {
  const asyncLocalStorage = new AsyncLocalStorage();
  const server = createServer((req, res) => {
    asyncLocalStorage.run({}, () => {
      getServe(getCLS, setCLS)(req, res);
    });
  });

  return {
    server,
    close
  };

  function getCLS() {
    const store = asyncLocalStorage.getStore();
    if (store === undefined) {
      return null;
    }
    return store.state;
  }

  function setCLS(state) {
    const store = asyncLocalStorage.getStore();
    if (store === undefined) {
      return;
    }
    store.state = state;
  }

  function close() {
    asyncLocalStorage.disable();
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
    setTimeout(() => {
      readFile(__filename, () => {
        res.setHeader('content-type', 'application/json');
        res.end(JSON.stringify({ cls: getCLS() }));
      });
    }, 10);
  };
}

const types = {
  'async-resource': buildCurrentResource,
  'destroy': buildDestroy,
  'async-local-storage': buildAsyncLocalStorage
};

const asyncMethods = {
  'callbacks': getServeCallbacks,
  'async': getServeAwait
};

function main({ type, asyncMethod, connections, duration, path }) {
  const { server, close } = types[type](asyncMethods[asyncMethod]);

  server
    .listen(common.PORT)
    .on('listening', () => {

      bench.http({
        path,
        connections,
        duration
      }, () => {
        close();
      });
    });
}
