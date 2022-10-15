// Flags: --experimental-synchronousworker
'use strict';

const common = require('../common');

const {
  strictEqual,
  throws,
} = require('node:assert');

const {
  SynchronousWorker,
} = require('node:worker_threads');

function deferred() {
  let res;
  const promise = new Promise((resolve) => res = resolve);
  return { res, promise };
}

{
  const w = new SynchronousWorker();

  w.runInWorkerScope(() => {
    const req = w.createRequire(__filename);
    const http = req('http');
    const vm = req('vm');
    const httpServer = http.createServer((req, res) => {
      if (req.url === '/stop') {
        w.stop();
      }
      res.writeHead(200);
      res.end('Ok\n');
    });

    // TODO(@jasnell): The original version of this test uses fetch() from node-fetch.
    // Trying to use the built-in fetch here, however, causes the test to hang because
    // the initial fetch() promise never resolves. Need to determine why.

    // httpServer.listen(0, req('vm').runInThisContext(`({fetch, httpServer }) => (async () => {
    //   const res = await fetch('http://localhost:' + httpServer.address().port + '/');
    //   globalThis.responseText = await res.text();
    //   await fetch('http://localhost:' + httpServer.address().port + '/stop');
    // })`)({ fetch: w.globalThis.fetch, httpServer }));

    httpServer.listen(0, vm.runInThisContext(`({fetch, httpServer, http }) => (async () => {
      const url = 'http://localhost:' + httpServer.address().port + '/';
      let resolver;
      const done = new Promise((resolve) => resolver = resolve);
      globalThis.responseText = '';
      http.get(url, (res) => {
        res.setEncoding('utf8');
        res.on('data', (chunk) => globalThis.responseText += chunk);
        res.on('end', () => {
          http.get(url + 'stop', () => {
            console.log('!!');
            resolver();
          });
        });
      });
      await done;
    })`)({ fetch, httpServer, http }));
  });

  strictEqual(w.loopAlive, true);
  w.runLoop('default');
  strictEqual(w.loopAlive, false);

  strictEqual(w.globalThis.responseText, 'Ok\n');
}

// With its own µtask queue'
{
  const w = new SynchronousWorker({ sharedEventLoop: true });
  let ran = false;
  w.runInWorkerScope(() => {
    w.globalThis.queueMicrotask(() => ran = true);
  });
  strictEqual(ran, true);
}

// With its own µtask queue but shared event loop
(function() {
  const w = new SynchronousWorker({ sharedEventLoop: true });
  let ran = false;
  w.runInWorkerScope(() => {
    w.globalThis.setImmediate(() => {
      w.globalThis.queueMicrotask(() => ran = true);
    });
  });
  strictEqual(ran, false);

  const { res, promise } = deferred();

  setImmediate(() => {
    strictEqual(ran, true);
    res();
  });

  return promise;
})().then(common.mustCall());

// With its own loop but shared µtask queue
{
  const w = new SynchronousWorker({ sharedMicrotaskQueue: true });
  let ran = false;
  w.runInWorkerScope(() => {
    w.globalThis.setImmediate(() => {
      w.globalThis.queueMicrotask(() => ran = true);
    });
  });

  strictEqual(ran, false);
  w.runLoop('default');
  strictEqual(ran, true);
}

// With its own loop but shared µtask queue (no callback scope)
(async function() {
  const w = new SynchronousWorker({ sharedMicrotaskQueue: true });
  let ran = false;
  w.globalThis.queueMicrotask(() => ran = true);
  strictEqual(ran, false);
  const { res, promise } = deferred();
  queueMicrotask(() => {
    strictEqual(ran, true);
    res();
  });
  return promise;
})().then(common.mustCall());

// Allows waiting for a specific promise to be resolved
{
  const w = new SynchronousWorker();
  const req = w.createRequire(__filename);
  let srv;
  let serverUpPromise;
  let fetchPromise;
  w.runInWorkerScope(() => {
    srv = req('http').createServer((req, res) => res.end('contents')).listen(0);
    serverUpPromise = req('events').once(srv, 'listening');
  });
  w.runLoopUntilPromiseResolved(serverUpPromise);
  w.runInWorkerScope(() => {
    const http = req('http');
    const { res, promise } = deferred();
    // TODO(@jasnell): The original version here used node-fetch. Using built in fetch
    // does not work for some reason...
    http.get('http://localhost:' + srv.address().port, (response) => {
      res({
        ok: true,
        status: response.statusCode,
      });
    });
    fetchPromise = promise;
  });
  const fetched = w.runLoopUntilPromiseResolved(fetchPromise);
  strictEqual(fetched.ok, true);
  strictEqual(fetched.status, 200);
}

// process.exit interrupts runInWorkerScope
{
  const w = new SynchronousWorker();
  let ranBefore = false;
  let ranAfter = false;
  let observedCode = -1;
  w.on('exit', (code) => observedCode = code);
  w.runInWorkerScope(() => {
    ranBefore = true;
    w.process.exit(1);
    ranAfter = true;
  });
  strictEqual(ranBefore, true);
  strictEqual(ranAfter, false);
  strictEqual(observedCode, 1);
}

// process.exit interrupts runLoop
{
  const w = new SynchronousWorker();
  let ranBefore = false;
  let ranAfter = false;
  let observedCode = -1;
  w.on('exit', (code) => observedCode = code);
  w.runInWorkerScope(() => {
    w.globalThis.setImmediate(() => {
      ranBefore = true;
      w.process.exit(1);
      ranAfter = true;
    });
  });
  w.runLoop('default');
  strictEqual(ranBefore, true);
  strictEqual(ranAfter, false);
  strictEqual(observedCode, 1);
}

// process.exit does not kill the process outside of any scopes
{
  const w = new SynchronousWorker();
  let observedCode = -1;

  w.on('exit', (code) => observedCode = code);
  w.process.exit(1);

  strictEqual(observedCode, 1);

  throws(() => {
    w.runLoop('default');
  }, /Worker has been stopped/);
}

// Allows adding uncaught exception listeners
{
  const w = new SynchronousWorker();
  let uncaughtException;
  let erroredOrExited = false;
  w.on('exit', () => erroredOrExited = true);
  w.on('errored', () => erroredOrExited = true);
  w.process.on('uncaughtException', (err) => uncaughtException = err);
  w.globalThis.setImmediate(() => { throw new Error('foobar'); });
  w.runLoop('default');
  strictEqual(erroredOrExited, false);
  strictEqual(uncaughtException.message, 'foobar');
}

// Handles entirely uncaught exceptions inside the loop well
{
  const w = new SynchronousWorker();
  let observedCode;
  let observedError;
  w.on('exit', (code) => observedCode = code);
  w.on('error', (error) => observedError = error);
  w.globalThis.setImmediate(() => { throw new Error('foobar'); });
  w.runLoop('default');
  strictEqual(observedCode, 1);
  strictEqual(observedError.message, 'foobar');
}

// Forbids nesting .runLoop() calls
{
  const w = new SynchronousWorker();
  let uncaughtException;
  w.process.on('uncaughtException', (err) => uncaughtException = err);
  w.globalThis.setImmediate(() => w.runLoop('default'));
  w.runLoop('default');
  strictEqual(uncaughtException.message, 'Cannot nest calls to runLoop');
}

// Properly handles timers that are about to expire when FreeEnvironment() is called on
// a shared event loop
(async function() {
  const w = new SynchronousWorker({
    sharedEventLoop: true,
    sharedMicrotaskQueue: true
  });

  setImmediate(() => {
    setTimeout(() => {}, 20);
    const now = Date.now();
    while (Date.now() - now < 30);
  });
  await w.stop();
})().then(common.mustCall());
