// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: global=window,dedicatedworker,sharedworker,serviceworker
// META: timeout=long

'use strict';

idl_test(
  ['web-locks'],
  ['html'],
  async idl_array => {
    idl_array.add_objects({
      LockManager: ['navigator.locks'],
      Lock: ['lock'],
    });

    if (self.Window) {
      idl_array.add_objects({ Navigator: ['navigator'] });
    } else {
      idl_array.add_objects({ WorkerNavigator: ['navigator'] });
    }

    try {
      await navigator.locks.request('name', l => { self.lock = l; });
    } catch (e) {
      // Surfaced in idlharness.js's test_object below.
    }
  }
);
