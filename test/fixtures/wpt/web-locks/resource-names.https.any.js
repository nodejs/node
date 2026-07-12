// META: title=Web Locks API: Resources DOMString edge cases
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

function code_points(s) {
  return [...s]
    .map(c => '0x' + c.charCodeAt(0).toString(16).toUpperCase())
    .join(' ');
}

[
  '', // Empty strings
  'abc\x00def', // Embedded NUL
  '\uD800', // Unpaired low surrogage
  '\uDC00', // Unpaired high surrogage
  '\uDC00\uD800', // Swapped surrogate pair
  '\uFFFF' // Non-character
].forEach(string => {
  promise_test(async t => {
    await navigator.locks.request(string, lock => {
      assert_equals(lock.name, string,
                          'Requested name matches granted name');
    });
  }, 'DOMString: ' + code_points(string));
});

promise_test(async t => {
  // '\uD800' treated as a USVString would become '\uFFFD'.
  await navigator.locks.request('\uD800', async lock => {
    assert_equals(lock.name, '\uD800');

    // |lock| is held for the duration of this name. It
    // Should not block acquiring |lock2| with a distinct
    // DOMString.
    await navigator.locks.request('\uFFFD', lock2 => {
      assert_equals(lock2.name, '\uFFFD');
    });

    // If we did not time out, this passed.
  });
}, 'Resource names that are not valid UTF-16 are not mangled');

promise_test(async t => {
  for (const name of ['-', '-foo']) {
    await promise_rejects_dom(
      t, 'NotSupportedError',
      navigator.locks.request(name, lock => {}),
      'Names starting with "-" should be rejected');
  }
  let got_lock = false;
  await navigator.locks.request('x-anything', lock => {
    got_lock = true;
  });
  assert_true(got_lock, 'Names with embedded "-" should be accepted');
}, 'Names cannot start with "-"');
