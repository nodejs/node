// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');

// Track getaddrinfo dispatch count and hold oncomplete callbacks so we can
// resolve them manually, simulating async libuv threadpool behaviour.
const pending = [];
let dispatchCount = 0;
cares.getaddrinfo = (req) => {
  dispatchCount++;
  pending.push(req);
  return 0; // Success - request is now "in flight"
};

const dns = require('dns');
const dnsPromises = dns.promises;

// ---------- promises API ----------

// Identical concurrent lookups should coalesce into a single getaddrinfo call.
async function testPromisesCoalesceIdentical() {
  dispatchCount = 0;
  pending.length = 0;

  const p1 = dnsPromises.lookup('coalesce-test.example');
  const p2 = dnsPromises.lookup('coalesce-test.example');
  const p3 = dnsPromises.lookup('coalesce-test.example');

  // Only one getaddrinfo should have been dispatched.
  assert.strictEqual(dispatchCount, 1);
  assert.strictEqual(pending.length, 1);

  // Complete the single underlying request.
  pending[0].oncomplete(null, ['1.2.3.4']);

  const [r1, r2, r3] = await Promise.all([p1, p2, p3]);
  assert.deepStrictEqual(r1, { address: '1.2.3.4', family: 4 });
  assert.deepStrictEqual(r2, { address: '1.2.3.4', family: 4 });
  assert.deepStrictEqual(r3, { address: '1.2.3.4', family: 4 });
}

// Lookups with different options must NOT coalesce.
async function testPromisesNoCoalesceDifferentOptions() {
  dispatchCount = 0;
  pending.length = 0;

  const p1 = dnsPromises.lookup('diff-opts.example');
  const p2 = dnsPromises.lookup('diff-opts.example', { family: 4 });
  const p3 = dnsPromises.lookup('other-host.example');

  // Lookups with different keys must not coalesce
  assert.strictEqual(dispatchCount, 3);

  // Complete all three.
  pending[0].oncomplete(null, ['1.1.1.1']);
  pending[1].oncomplete(null, ['2.2.2.2']);
  pending[2].oncomplete(null, ['3.3.3.3']);

  const [r1, r2, r3] = await Promise.all([p1, p2, p3]);
  assert.deepStrictEqual(r1, { address: '1.1.1.1', family: 4 });
  assert.deepStrictEqual(r2, { address: '2.2.2.2', family: 4 });
  assert.deepStrictEqual(r3, { address: '3.3.3.3', family: 4 });
}

// Error should propagate to all coalesced callers.
async function testPromisesCoalesceError() {
  dispatchCount = 0;
  pending.length = 0;

  const p1 = dnsPromises.lookup('fail-coalesce.example');
  const p2 = dnsPromises.lookup('fail-coalesce.example');

  assert.strictEqual(dispatchCount, 1);

  pending[0].oncomplete(internalBinding('uv').UV_EAI_AGAIN);

  const expected = { code: 'EAI_AGAIN', syscall: 'getaddrinfo' };
  await assert.rejects(p1, expected);
  await assert.rejects(p2, expected);
}

// Coalesced lookups with all:true and all:false on same key should each get
// correctly post-processed results.
async function testPromisesCoalesceMixedAll() {
  dispatchCount = 0;
  pending.length = 0;

  const p1 = dnsPromises.lookup('mixed.example');
  const p2 = dnsPromises.lookup('mixed.example', { all: true });

  // all:true vs all:false share the same (hostname, family=0, hints=0, order)
  // key, so they must coalesce.
  assert.strictEqual(dispatchCount, 1);

  pending[0].oncomplete(null, ['10.0.0.1', '10.0.0.2']);

  const [r1, r2] = await Promise.all([p1, p2]);
  // all:false → first address only
  assert.deepStrictEqual(r1, { address: '10.0.0.1', family: 4 });
  // all:true → array of all addresses
  assert.deepStrictEqual(r2, [
    { address: '10.0.0.1', family: 4 },
    { address: '10.0.0.2', family: 4 },
  ]);
}

// After a coalesced batch settles, a new lookup for the same key should
// dispatch a fresh getaddrinfo (the inflight entry must be cleaned up).
async function testPromisesCoalesceCleanup() {
  dispatchCount = 0;
  pending.length = 0;

  const p1 = dnsPromises.lookup('cleanup.example');
  assert.strictEqual(dispatchCount, 1);
  pending[0].oncomplete(null, ['5.5.5.5']);
  await p1;

  // Second wave — must dispatch a new getaddrinfo.
  const p2 = dnsPromises.lookup('cleanup.example');
  assert.strictEqual(dispatchCount, 2);
  pending[1].oncomplete(null, ['6.6.6.6']);
  const r2 = await p2;
  assert.deepStrictEqual(r2, { address: '6.6.6.6', family: 4 });
}

// ---------- callback API ----------

// Identical concurrent callback-based lookups should coalesce.
async function testCallbackCoalesceIdentical() {
  dispatchCount = 0;
  pending.length = 0;

  const results = [];
  const done = new Promise((resolve) => {
    let remaining = 3;
    function cb(err, address, family) {
      results.push({ err, address, family });
      if (--remaining === 0) resolve();
    }
    dns.lookup('cb-coalesce.example', cb);
    dns.lookup('cb-coalesce.example', cb);
    dns.lookup('cb-coalesce.example', cb);
  });

  // Callback lookups must coalesce
  assert.strictEqual(dispatchCount, 1);

  pending[0].oncomplete(null, ['7.7.7.7']);
  await done;

  for (const r of results) {
    assert.strictEqual(r.err, null);
    assert.strictEqual(r.address, '7.7.7.7');
    assert.strictEqual(r.family, 4);
  }
}

// Callback error propagation to all coalesced callers.
async function testCallbackCoalesceError() {
  dispatchCount = 0;
  pending.length = 0;

  const results = [];
  const done = new Promise((resolve) => {
    let remaining = 2;
    function cb(err) {
      results.push(err);
      if (--remaining === 0) resolve();
    }
    dns.lookup('cb-fail.example', cb);
    dns.lookup('cb-fail.example', cb);
  });

  assert.strictEqual(dispatchCount, 1);

  pending[0].oncomplete(internalBinding('uv').UV_EAI_AGAIN);
  await done;

  for (const err of results) {
    assert.strictEqual(err.code, 'EAI_AGAIN');
    assert.strictEqual(err.syscall, 'getaddrinfo');
  }
}

// ---------- async context (AsyncLocalStorage) ----------

// Callback API: each coalesced caller must retain its own async context.
async function testCallbackAsyncContext() {
  dispatchCount = 0;
  pending.length = 0;

  const als = new AsyncLocalStorage();
  const results = await new Promise((resolve) => {
    const out = [];
    let remaining = 3;
    function done() { if (--remaining === 0) resolve(out); }

    als.run('ctx-A', () => {
      dns.lookup('async-ctx.example', () => {
        out.push({ id: 'A', ctx: als.getStore() });
        done();
      });
    });
    als.run('ctx-B', () => {
      dns.lookup('async-ctx.example', () => {
        out.push({ id: 'B', ctx: als.getStore() });
        done();
      });
    });
    als.run('ctx-C', () => {
      dns.lookup('async-ctx.example', () => {
        out.push({ id: 'C', ctx: als.getStore() });
        done();
      });
    });

    assert.strictEqual(dispatchCount, 1);
    pending[0].oncomplete(null, ['1.2.3.4']);
  });

  for (const r of results) {
    assert.strictEqual(r.ctx, `ctx-${r.id}`,
                       `callback ${r.id} lost its async context`);
  }
}

// Promises API: each coalesced caller must retain its own async context.
async function testPromisesAsyncContext() {
  dispatchCount = 0;
  pending.length = 0;

  const als = new AsyncLocalStorage();
  const p1 = als.run('ctx-A', () =>
    dnsPromises.lookup('async-ctx-p.example').then(
      (r) => ({ ...r, ctx: als.getStore() })));
  const p2 = als.run('ctx-B', () =>
    dnsPromises.lookup('async-ctx-p.example').then(
      (r) => ({ ...r, ctx: als.getStore() })));

  assert.strictEqual(dispatchCount, 1);
  pending[0].oncomplete(null, ['2.3.4.5']);

  const [r1, r2] = await Promise.all([p1, p2]);
  assert.strictEqual(r1.ctx, 'ctx-A');
  assert.strictEqual(r2.ctx, 'ctx-B');
}

(async () => {
  await testPromisesCoalesceIdentical();
  await testPromisesNoCoalesceDifferentOptions();
  await testPromisesCoalesceError();
  await testPromisesCoalesceMixedAll();
  await testPromisesCoalesceCleanup();
  await testCallbackCoalesceIdentical();
  await testCallbackCoalesceError();
  await testCallbackAsyncContext();
  await testPromisesAsyncContext();
})().then(common.mustCall());
