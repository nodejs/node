// META: title=Web Locks API: navigator.locks.query method
// META: script=resources/helpers.js

'use strict';

// Returns an array of the modes for the locks with matching name.
function modes(list, name) {
  return list.filter(item => item.name === name).map(item => item.mode);
}
// Returns an array of the clientIds for the locks with matching name.
function clients(list, name) {
  return list.filter(item => item.name === name).map(item => item.clientId);
}

promise_test(async t => {
  const res = uniqueName(t);

  await navigator.locks.request(res, async lock1 => {
    // Attempt to request this again - should be blocked.
    let lock2_acquired = false;
    navigator.locks.request(res, lock2 => { lock2_acquired = true; });

    // Verify that it was blocked.
    await navigator.locks.request(res, {ifAvailable: true}, async lock3 => {
      assert_false(lock2_acquired, 'second request should be blocked');
      assert_equals(lock3, null, 'third request should have failed');

      const state = await navigator.locks.query();

      assert_own_property(state, 'pending', 'State has `pending` property');
      assert_true(Array.isArray(state.pending),
                  'State `pending` property is an array');
      const pending_info = state.pending[0];
      assert_own_property(pending_info, 'name',
                          'Pending info dictionary has `name` property');
      assert_own_property(pending_info, 'mode',
                          'Pending info dictionary has `mode` property');
      assert_own_property(pending_info, 'clientId',
                          'Pending info dictionary has `clientId` property');

      assert_own_property(state, 'held', 'State has `held` property');
      assert_true(Array.isArray(state.held),
                  'State `held` property is an array');
      const held_info = state.held[0];
      assert_own_property(held_info, 'name',
                          'Held info dictionary has `name` property');
      assert_own_property(held_info, 'mode',
                          'Held info dictionary has `mode` property');
      assert_own_property(held_info, 'clientId',
                          'Held info dictionary has `clientId` property');
    });
  });
}, 'query() returns dictionaries with expected properties');



promise_test(async t => {
  const res = uniqueName(t);

  await navigator.locks.request(res, async lock1 => {
    const state = await navigator.locks.query();
    assert_array_equals(modes(state.held, res), ['exclusive'],
                        'Held lock should appear once');
  });

  await navigator.locks.request(res, {mode: 'shared'}, async lock1 => {
    const state = await navigator.locks.query();
    assert_array_equals(modes(state.held, res), ['shared'],
                        'Held lock should appear once');
  });
}, 'query() reports individual held locks');

promise_test(async t => {
  const res1 = uniqueName(t);
  const res2 = uniqueName(t);

  await navigator.locks.request(res1, async lock1 => {
    await navigator.locks.request(res2, {mode: 'shared'}, async lock2 => {
      const state = await navigator.locks.query();
      assert_array_equals(modes(state.held, res1), ['exclusive'],
                          'Held lock should appear once');
      assert_array_equals(modes(state.held, res2), ['shared'],
                          'Held lock should appear once');
    });
  });
}, 'query() reports multiple held locks');

promise_test(async t => {
  const res = uniqueName(t);

  await navigator.locks.request(res, async lock1 => {
    // Attempt to request this again - should be blocked.
    let lock2_acquired = false;
    navigator.locks.request(res, lock2 => { lock2_acquired = true; });

    // Verify that it was blocked.
    await navigator.locks.request(res, {ifAvailable: true}, async lock3 => {
      assert_false(lock2_acquired, 'second request should be blocked');
      assert_equals(lock3, null, 'third request should have failed');

      const state = await navigator.locks.query();
      assert_array_equals(modes(state.pending, res), ['exclusive'],
                          'Pending lock should appear once');
      assert_array_equals(modes(state.held, res), ['exclusive'],
                          'Held lock should appear once');
    });
  });
}, 'query() reports pending and held locks');

promise_test(async t => {
  const res = uniqueName(t);

  await navigator.locks.request(res, {mode: 'shared'}, async lock1 => {
    await navigator.locks.request(res, {mode: 'shared'}, async lock2 => {
      const state = await navigator.locks.query();
      assert_array_equals(modes(state.held, res), ['shared', 'shared'],
                          'Held lock should appear twice');
    });
  });
}, 'query() reports held shared locks with appropriate count');

promise_test(async t => {
  const res = uniqueName(t);

  await navigator.locks.request(res, async lock1 => {
    let lock2_acquired = false, lock3_acquired = false;
    navigator.locks.request(res, {mode: 'shared'},
                            lock2 => { lock2_acquired = true; });
    navigator.locks.request(res, {mode: 'shared'},
                            lock3 => { lock3_acquired = true; });

    await navigator.locks.request(res, {ifAvailable: true}, async lock4 => {
      assert_equals(lock4, null, 'lock should not be available');
      assert_false(lock2_acquired, 'second attempt should be blocked');
      assert_false(lock3_acquired, 'third attempt should be blocked');

      const state = await navigator.locks.query();
      assert_array_equals(modes(state.held, res), ['exclusive'],
                          'Held lock should appear once');

      assert_array_equals(modes(state.pending, res), ['shared', 'shared'],
                          'Pending lock should appear twice');
    });
  });
}, 'query() reports pending shared locks with appropriate count');

promise_test(async t => {
  const res1 = uniqueName(t);
  const res2 = uniqueName(t);

  await navigator.locks.request(res1, async lock1 => {
    await navigator.locks.request(res2, async lock2 => {
      const state = await navigator.locks.query();

      const res1_clients = clients(state.held, res1);
      const res2_clients = clients(state.held, res2);

      assert_equals(res1_clients.length, 1, 'Each lock should have one holder');
      assert_equals(res2_clients.length, 1, 'Each lock should have one holder');

      assert_array_equals(res1_clients, res2_clients,
                          'Both locks should have same clientId');
    });
  });
}, 'query() reports the same clientId for held locks from the same context');

promise_test(async t => {
  const res = uniqueName(t);

  const worker = new Worker('resources/worker.js');
  t.add_cleanup(() => { worker.terminate(); });

  await postToWorkerAndWait(
    worker, {op: 'request', name: res, mode: 'shared'});

  await navigator.locks.request(res, {mode: 'shared'}, async lock => {
    const state = await navigator.locks.query();
    const res_clients = clients(state.held, res);
    assert_equals(res_clients.length, 2, 'Clients should have same resource');
    assert_not_equals(res_clients[0], res_clients[1],
                      'Clients should have different ids');
  });
}, 'query() reports different ids for held locks from different contexts');

promise_test(async t => {
  const res1 = uniqueName(t);
  const res2 = uniqueName(t);

  const worker = new Worker('resources/worker.js');
  t.add_cleanup(() => { worker.terminate(); });

  // Acquire 1 in the worker.
  await postToWorkerAndWait(worker, {op: 'request', name: res1})

  // Acquire 2 here.
  await new Promise(resolve => {
    navigator.locks.request(res2, lock => {
      resolve();
      return new Promise(() => {}); // Never released.
    });
  });

  // Request 2 in the worker.
  postToWorkerAndWait(worker, {op: 'request', name: res2});
  assert_true((await postToWorkerAndWait(worker, {
    op: 'request', name: res2, ifAvailable: true
  })).failed, 'Lock request should have failed');

  // Request 1 here.
  navigator.locks.request(
    res1, t.unreached_func('Lock should not be acquired'));

  // Verify that we're seeing a deadlock.
  const state = await navigator.locks.query();
  const res1_held_clients = clients(state.held, res1);
  const res2_held_clients = clients(state.held, res2);
  const res1_pending_clients = clients(state.pending, res1);
  const res2_pending_clients = clients(state.pending, res2);

  assert_equals(res1_held_clients.length, 1);
  assert_equals(res2_held_clients.length, 1);
  assert_equals(res1_pending_clients.length, 1);
  assert_equals(res2_pending_clients.length, 1);

  assert_equals(res1_held_clients[0], res2_pending_clients[0]);
  assert_equals(res2_held_clients[0], res1_pending_clients[0]);
}, 'query() can observe a deadlock');
