// META: title=Web Locks API: Storage Buckets have independent lock sets
// META: script=resources/helpers.js
// META: script=/storage/buckets/resources/util.js
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

/**
 * Returns whether bucket1 and bucket2 share locks
 * @param {*} t test runner object
 * @param {*} bucket1 Storage bucket
 * @param {*} bucket2 Storage bucket
 */
async function locksAreShared(t, bucket1, bucket2) {
  const lock_name = self.uniqueName(t);
  let callback_called = false;
  let locks_are_shared;
  await bucket1.locks.request(lock_name, async lock => {
    await bucket2.locks.request(
      lock_name, { ifAvailable: true }, async lock => {
        callback_called = true;
        locks_are_shared = lock == null;
      });
  });
  assert_true(callback_called, 'callback should be called');
  return locks_are_shared;
}

promise_test(async t => {
  await prepareForBucketTest(t);

  const inboxBucket = await navigator.storageBuckets.open('inbox');
  const draftsBucket = await navigator.storageBuckets.open('drafts');

  assert_true(
    await locksAreShared(t, navigator, navigator),
    'The default bucket should share locks with itself');

  assert_true(
    await locksAreShared(t, inboxBucket, inboxBucket),
    'A non default bucket should share locks with itself');

  assert_false(
    await locksAreShared(t, navigator, inboxBucket),
    'The default bucket shouldn\'t share locks with a non default bucket');

  assert_false(
    await locksAreShared(t, draftsBucket, inboxBucket),
    'Two different non default buckets shouldn\'t share locks');

  const inboxBucket2 = await navigator.storageBuckets.open('inbox');

  assert_true(
    await self.locksAreShared(t, inboxBucket, inboxBucket2),
    'A two instances of the same non default bucket should share locks with theirselves');
}, 'Storage buckets have independent locks');
