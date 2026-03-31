// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');

// Stub `getaddrinfo` to proxy its call to a dynamic stub. This has to be done
// before we load the `dns` module to guarantee that the `dns` module uses the
// stub.
let getaddrinfoStub = null;
cares.getaddrinfo = (req) => getaddrinfoStub(req);

const dns = require('dns');
const dnsPromises = dns.promises;

// dns.promises.lookup — already-aborted signal rejects immediately without
// calling getaddrinfo
async function promisesLookupAlreadyAborted() {
  getaddrinfoStub = common.mustNotCall(
    'getaddrinfo must not be called when signal is already aborted',
  );
  const ac = new AbortController();
  ac.abort('my reason');
  await assert.rejects(
    dnsPromises.lookup('example.com', { signal: ac.signal }),
    (err) => {
      assert.strictEqual(err.name, 'AbortError');
      assert.strictEqual(err.cause, 'my reason');
      return true;
    },
  );
}

// dns.promises.lookup — abort during pending request rejects, and the late
// oncomplete is silently ignored (no double-settle)
async function promisesLookupAbortDuringPending() {
  const ac = new AbortController();
  let savedReq;
  getaddrinfoStub = common.mustCall((req) => {
    savedReq = req;
    return 0;
  });
  const promise = dnsPromises.lookup('example.com', { signal: ac.signal });
  ac.abort('cancelled');
  await assert.rejects(
    promise,
    (err) => {
      assert.strictEqual(err.name, 'AbortError');
      assert.strictEqual(err.cause, 'cancelled');
      return true;
    },
  );
  // Simulate the late oncomplete arriving after abort — must not throw or
  // cause an unhandled rejection.
  savedReq.oncomplete(null, ['127.0.0.1']);
}

// dns.promises.lookup — signal not aborted, request completes normally and
// the abort listener is removed from the signal
async function promisesLookupNormalCompletion() {
  const ac = new AbortController();
  getaddrinfoStub = common.mustCall((req) => {
    req.oncomplete(null, ['127.0.0.1']);
    return 0;
  });
  const result = await dnsPromises.lookup('example.com', { signal: ac.signal });
  assert.deepStrictEqual(result, { address: '127.0.0.1', family: 4 });
  // The abort listener must have been cleaned up — aborting now must not
  // trigger any side-effects.
  ac.abort();
}

// dns.lookup (callback) — already-aborted signal calls back with AbortError
// without calling getaddrinfo
async function callbackLookupAlreadyAborted() {
  getaddrinfoStub = common.mustNotCall(
    'getaddrinfo must not be called when signal is already aborted',
  );
  const ac = new AbortController();
  ac.abort('my reason');
  return new Promise((resolve) => {
    dns.lookup('example.com', { signal: ac.signal }, common.mustCall((err) => {
      assert.strictEqual(err.name, 'AbortError');
      assert.strictEqual(err.cause, 'my reason');
      resolve();
    }));
  });
}

// dns.lookup (callback) — abort during pending request calls back with
// AbortError, and late oncomplete is silently ignored
async function callbackLookupAbortDuringPending() {
  const ac = new AbortController();
  let savedReq;
  getaddrinfoStub = common.mustCall((req) => {
    savedReq = req;
    return 0;
  });
  return new Promise((resolve) => {
    dns.lookup('example.com', { signal: ac.signal },
               common.mustCall((err) => {
                 assert.strictEqual(err.name, 'AbortError');
                 assert.strictEqual(err.cause, 'cancelled');
                 // Simulate late oncomplete — must not call the callback again
                 // (mustCall already asserts exactly-once).
                 savedReq.oncomplete(null, ['127.0.0.1']);
                 resolve();
               }));
    ac.abort('cancelled');
  });
}

// dns.lookup (callback) — signal not aborted, completes normally and the
// abort listener is removed
async function callbackLookupNormalCompletion() {
  const ac = new AbortController();
  getaddrinfoStub = common.mustCall((req) => {
    req.oncomplete(null, ['127.0.0.1']);
    return 0;
  });
  return new Promise((resolve) => {
    dns.lookup('example.com', { signal: ac.signal },
               common.mustCall((err, address, family) => {
                 assert.ifError(err);
                 assert.strictEqual(address, '127.0.0.1');
                 assert.strictEqual(family, 4);
                 // Aborting after completion must not trigger any side-effects.
                 ac.abort();
                 resolve();
               }));
  });
}

// dns.promises.lookup — without signal, lookup still works
async function promisesLookupWithoutSignal() {
  getaddrinfoStub = common.mustCall((req) => {
    req.oncomplete(null, ['127.0.0.1']);
    return 0;
  });
  const result = await dnsPromises.lookup('example.com');
  assert.deepStrictEqual(result, { address: '127.0.0.1', family: 4 });
}

// dns.promises.lookup — invalid signal type throws
async function promisesLookupInvalidSignal() {
  assert.throws(
    () => dnsPromises.lookup('example.com', { signal: 'not-a-signal' }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

// dns.lookup — invalid signal type throws
async function callbackLookupInvalidSignal() {
  assert.throws(
    () => dns.lookup('example.com', { signal: 'not-a-signal' },
                     common.mustNotCall()),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

(async () => {
  await promisesLookupAlreadyAborted();
  await promisesLookupAbortDuringPending();
  await promisesLookupNormalCompletion();
  await callbackLookupAlreadyAborted();
  await callbackLookupAbortDuringPending();
  await callbackLookupNormalCompletion();
  await promisesLookupWithoutSignal();
  await promisesLookupInvalidSignal();
  await callbackLookupInvalidSignal();
})().then(common.mustCall());
