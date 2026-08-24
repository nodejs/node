'use strict';

// Returns a promise for a network response that contains the Clear-Site-Data:
// "storage" header.
function clear_site_data() {
  return fetch('resources/blank.html?pipe=header(Clear-Site-Data,"storage")');
}

async function assert_no_registrations_exist() {
  const registrations = await navigator.serviceWorker.getRegistrations();
  assert_equals(registrations.length, 0);
}

async function add_controlled_iframe(test, url) {
  const frame = await with_iframe(url);
  test.add_cleanup(() => { frame.remove(); });
  assert_not_equals(frame.contentWindow.navigator.serviceWorker.controller, null);
  return frame;
}
