// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestRealmMicrotaskQueue() {
  let log = [];
  Realm.shared = log;

  // Create realms with custom and default MicrotaskQueues.
  const r_custom = Realm.create({create_own_microtask_queue: true});
  const r_default = Realm.create();

  // 1. Enqueue in custom realm (handler context = r_custom -> custom MQ).
  Realm.eval(
      r_custom, 'Promise.resolve().then(() => Realm.shared.push(\'custom\'))');

  // 2. Enqueue in default realm (handler context = r_default -> default MQ).
  Realm.eval(
      r_default,
      'Promise.resolve().then(() => Realm.shared.push(\'default\'))');

  // 3. Enqueue in main (handler context = main -> default MQ).
  Promise.resolve().then(() => log.push('main'));

  // Assert that immediately, nothing has run yet because main script is still
  // running.
  assertEquals([], log);

  // 4. Inspect log during default queue checkpoint.
  Promise.resolve().then(() => {
    // The default microtask queue flushes 'default' and 'main'.
    // 'custom' is NOT flushed because it is trapped in r_custom's isolated
    // microtask queue.
    assertEquals(['default', 'main'], log);
  });
})();
