'use strict';

/* eslint-disable no-void */

// PGO Training Script: Async Patterns, Timers, and Events
//
// Modern Node.js code is dominated by async/await and Promises.
// This script exercises:
// - Promise creation, chaining, and resolution (every async operation)
// - async/await control flow (the primary coding pattern)
// - Promise.all/allSettled/race/any (concurrent operation patterns)
// - EventEmitter (backbone of all Node.js I/O)
// - Timers: setTimeout, setInterval, setImmediate (scheduling)
// - AbortController/AbortSignal (cancellation - growing usage)
// - queueMicrotask / process.nextTick (microtask scheduling)
// - AsyncLocalStorage (request context propagation - growing fast)
//
// This exercises: V8 Promise machinery, microtask queue, libuv timer heap,
// EventEmitter C++/JS boundary, AbortSignal C++ implementation.

const { EventEmitter } = require('events');
const { AsyncLocalStorage } = require('async_hooks');
const {
  setTimeout: setTimeoutPromise,
  setImmediate: setImmediatePromise,
} = require('timers/promises');

const DURATION_MS = parseInt(process.env.PGO_TRAINING_DURATION, 10) || 15_000;

// Workload 1: Promise chains (REST API middleware pattern)
async function workloadPromiseChains(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Simple promise chain (Express middleware-like)
    await Promise.resolve({ method: 'GET', url: '/api/users' })
      .then((req) => ({ ...req, authenticated: true }))
      .then((req) => ({ ...req, parsed: true, body: {} }))
      .then((req) => ({ ...req, validated: true }))
      .then(() => ({
        status: 200,
        body: JSON.stringify({ users: [], total: 0 }),
        headers: { 'content-type': 'application/json' },
      }));
    ops++;

    // Promise with error handling (try/catch in async flow)
    try {
      await Promise.resolve(i)
        .then((v) => {
          if (v % 7 === 0) throw new Error('validation');
          return v;
        })
        .then((v) => v * 2)
        .catch((err) => ({ error: err.message }));
      ops++;
    } catch {
      ops++;
    }

    // Nested promise resolution (database query pattern)
    const result = await new Promise((resolve) => {
      // Simulate async work
      resolve({
        rows: Array.from({ length: 10 }, (_, j) => ({ id: j, value: i + j })),
      });
    });
    await new Promise((resolve) => {
      resolve(result.rows.map((r) => ({ ...r, processed: true })));
    });
    ops++;
  }
  return ops;
}

// Workload 2: Promise.all / allSettled / race / any (concurrent patterns)
async function workloadPromiseConcurrency(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Promise.all (parallel database queries, batch API calls)
    await Promise.all(
      Array.from({ length: 20 }, (_, j) =>
        Promise.resolve({ id: j, data: `result_${j}` }),
      ),
    );
    ops++;

    // Promise.allSettled (fault-tolerant batch operations)
    await Promise.allSettled(
      Array.from({ length: 15 }, (_, j) =>
        (j % 5 === 0 ?
          Promise.reject(new Error(`fail_${j}`)) :
          Promise.resolve({ id: j, ok: true })),
      ),
    );
    ops++;

    // Promise.race (timeout pattern)
    await Promise.race([
      Promise.resolve('fast'),
      new Promise((resolve) => setTimeout(resolve, 10000, 'slow')),
    ]);
    ops++;

    // Promise.any (failover pattern)
    await Promise.any([
      Promise.reject(new Error('server1')),
      Promise.resolve('server2'),
      Promise.resolve('server3'),
    ]);
    ops++;

    // Batched parallel with limit (connection pool pattern)
    const batchSize = 5;
    const items = Array.from({ length: 20 }, (_, j) => j);
    for (let start = 0; start < items.length; start += batchSize) {
      const batch = items.slice(start, start + batchSize);
      await Promise.all(batch.map((item) => Promise.resolve(item * 2)));
    }
    ops++;
  }
  return ops;
}

// Workload 3: EventEmitter (core Node.js pattern)
function workloadEventEmitter(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const emitter = new EventEmitter();

    // Add multiple listeners (typical server setup)
    const listeners = [];
    for (let j = 0; j < 10; j++) {
      const listener = (data) => {
        data.processed = true;
      };
      listeners.push(listener);
      emitter.on('data', listener);
    }
    ops++;

    // Once listener (connection setup pattern)
    emitter.once('connect', () => {});
    emitter.once('ready', () => {});
    ops++;

    // Emit events (hot path in I/O)
    for (let j = 0; j < 100; j++) {
      emitter.emit('data', { id: j, value: j * 2 });
    }
    ops += 100;

    // Emit with multiple arguments
    for (let j = 0; j < 20; j++) {
      emitter.emit('data', { id: j }, 'extra', j);
    }
    ops += 20;

    // Error event handling
    emitter.on('error', () => {});
    emitter.emit('error', new Error('test'));
    ops++;

    // listenerCount / listeners (monitoring)
    emitter.listenerCount('data');
    emitter.listeners('data');
    emitter.rawListeners('data');
    emitter.eventNames();
    ops += 4;

    // Remove listeners (cleanup)
    for (const listener of listeners) {
      emitter.removeListener('data', listener);
    }
    emitter.removeAllListeners();
    ops++;
  }
  return ops;
}

// Workload 4: EventTarget (Web API compatibility — growing usage)
function workloadEventTarget(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    const target = new EventTarget();

    // Add listeners
    const handlers = [];
    for (let j = 0; j < 5; j++) {
      const handler = (e) => {
        void e.detail;
      };
      handlers.push(handler);
      target.addEventListener('message', handler);
    }
    ops++;

    // Dispatch events
    for (let j = 0; j < 50; j++) {
      target.dispatchEvent(new Event('message'));
    }
    ops += 50;

    // Remove listeners
    for (const handler of handlers) {
      target.removeEventListener('message', handler);
    }
    ops++;
  }
  return ops;
}

// Workload 5: Timers (setTimeout, setInterval, setImmediate)
async function workloadTimers(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // setTimeout with 0 (defer to next event loop - very common)
    await new Promise((resolve) => setTimeout(resolve, 0));
    ops++;

    // setImmediate (process next I/O callbacks first)
    await new Promise((resolve) => setImmediate(resolve));
    ops++;

    // setTimeout promise API
    await setTimeoutPromise(0);
    ops++;

    // setImmediate promise API
    await setImmediatePromise();
    ops++;

    // Timer creation and cancellation (debounce/throttle patterns)
    for (let j = 0; j < 10; j++) {
      const timer = setTimeout(() => {}, 10000);
      clearTimeout(timer);
    }
    ops += 10;

    // setInterval + clearInterval (polling pattern)
    const interval = setInterval(() => {}, 1000);
    clearInterval(interval);
    ops++;

    // process.nextTick (microtask — higher priority than timers)
    await new Promise((resolve) => process.nextTick(resolve));
    ops++;

    // queueMicrotask
    await new Promise((resolve) => queueMicrotask(resolve));
    ops++;

    // Nested nextTick/microtask (realistic: multiple middleware layers)
    await new Promise((outerResolve) => {
      process.nextTick(() => {
        queueMicrotask(() => {
          process.nextTick(() => {
            outerResolve();
          });
        });
      });
    });
    ops++;
  }
  return ops;
}

// Workload 6: AbortController (cancellation — widely used since Node.js 16+)
async function workloadAbortController(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Create and signal (timeout pattern)
    const ac1 = new AbortController();
    const { signal: sig1 } = ac1;
    sig1.addEventListener('abort', () => {});
    ac1.abort();
    ops++;

    // AbortSignal.timeout (modern API)
    const sig2 = AbortSignal.timeout(5000);
    void sig2.aborted; // check status
    ops++;

    // AbortSignal.any (composite signals)
    const ac3 = new AbortController();
    const sig3 = AbortSignal.any([ac3.signal, AbortSignal.timeout(10000)]);
    sig3.addEventListener('abort', () => {});
    ac3.abort('cancelled');
    ops++;

    // Using with setTimeout (common pattern)
    try {
      const ac = new AbortController();
      const timer = setTimeout(() => ac.abort(), 0);
      await setTimeoutPromise(0, undefined, { signal: ac.signal });
      clearTimeout(timer);
      ops++;
    } catch {
      ops++; // AbortError is expected sometimes
    }
  }
  return ops;
}

// Workload 7: AsyncLocalStorage (request context — Express, Fastify, Nest.js)
async function workloadAsyncLocalStorage(iterations) {
  let ops = 0;
  const als = new AsyncLocalStorage();

  for (let i = 0; i < iterations; i++) {
    // Run with context (HTTP request lifecycle)
    await als.run(
      { requestId: `req-${i}`, userId: `user-${i % 100}` },
      async () => {
        const store = als.getStore();

        // "Middleware" that reads context
        void store.requestId;

        // "Service" layer — context propagates through async calls
        await Promise.resolve().then(() => {
          const ctx = als.getStore();
          return { ...ctx, processed: true };
        });

        // Nested async operation with context
        await new Promise((resolve) => {
          setImmediate(() => {
            const ctx = als.getStore();
            resolve(ctx);
          });
        });

        ops++;
      },
    );

    // enterWith pattern (alternative API)
    als.enterWith({ contextId: i });
    als.getStore();
    ops++;
  }
  return ops;
}

// Workload 8: Error creation and stack traces (very frequent in real apps)
function workloadErrors(iterations) {
  let ops = 0;

  for (let i = 0; i < iterations; i++) {
    // Error creation with stack trace
    const err1 = new Error(`Something went wrong: ${i}`);
    void err1.stack; // Access stack (forces stack trace computation)
    void err1.message;
    ops++;

    // TypeError (most common runtime error)
    const err2 = new TypeError(`Cannot read property of undefined`);
    void err2.stack;
    ops++;

    // RangeError
    const err3 = new RangeError(`Value out of range: ${i}`);
    err3.code = 'ERR_OUT_OF_RANGE';
    void err3.stack;
    ops++;

    // try/catch (V8's exception handling path)
    for (let j = 0; j < 10; j++) {
      try {
        if (j % 3 === 0) throw new Error(`Caught error ${j}`);
        if (j % 7 === 0) throw new TypeError(`Type error ${j}`);
      } catch (e) {
        void e.message; // Access message
      }
    }
    ops += 10;

    // Error.captureStackTrace (custom errors pattern)
    class AppError extends Error {
      constructor(message, code) {
        super(message);
        this.code = code;
        Error.captureStackTrace(this, AppError);
      }
    }
    const err4 = new AppError('Not found', 404);
    void err4.stack;
    ops++;
  }
  return ops;
}

async function main() {
  console.log('[pgo-async-patterns] Starting async patterns workload...');
  const startTime = Date.now();
  let totalOps = 0;
  let round = 0;

  const remaining = () => DURATION_MS - (Date.now() - startTime);

  while (remaining() > 0) {
    round++;
    const scale = Math.max(0.1, remaining() / DURATION_MS);
    const iterScale = (base) => Math.max(1, Math.floor(base * scale));

    // Promise chains (extremely high frequency)
    if (round === 1)
      console.log('[pgo-async-patterns] Running promise chains...');
    totalOps += await workloadPromiseChains(iterScale(500));
    if (remaining() <= 0) break;

    // Promise concurrency
    if (round === 1)
      console.log('[pgo-async-patterns] Running promise concurrency...');
    totalOps += await workloadPromiseConcurrency(iterScale(200));
    if (remaining() <= 0) break;

    // EventEmitter (core pattern)
    if (round === 1)
      console.log('[pgo-async-patterns] Running EventEmitter...');
    totalOps += workloadEventEmitter(iterScale(200));
    if (remaining() <= 0) break;

    // EventTarget
    if (round === 1) console.log('[pgo-async-patterns] Running EventTarget...');
    totalOps += workloadEventTarget(iterScale(100));
    if (remaining() <= 0) break;

    // Timers
    if (round === 1) console.log('[pgo-async-patterns] Running timers...');
    totalOps += await workloadTimers(iterScale(100));
    if (remaining() <= 0) break;

    // AbortController
    if (round === 1)
      console.log('[pgo-async-patterns] Running AbortController...');
    totalOps += await workloadAbortController(iterScale(100));
    if (remaining() <= 0) break;

    // AsyncLocalStorage
    if (round === 1)
      console.log('[pgo-async-patterns] Running AsyncLocalStorage...');
    totalOps += await workloadAsyncLocalStorage(iterScale(100));
    if (remaining() <= 0) break;

    // Errors
    if (round === 1)
      console.log('[pgo-async-patterns] Running error patterns...');
    totalOps += workloadErrors(iterScale(300));
  }

  const elapsed = (Date.now() - startTime) / 1000;
  console.log(
    `[pgo-async-patterns] Completed ${totalOps} ops in ${elapsed.toFixed(1)}s (${(totalOps / elapsed).toFixed(0)} ops/s) [${round} rounds]`,
  );
}

main().catch((err) => {
  console.error('[pgo-async-patterns] Error:', err);
  process.exit(1);
});
