// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

// https://w3c.github.io/device-memory/

"use strict";

idl_test(
  ['device-memory'],
  ['html'],
  async idl_array => {
    if (self.GLOBAL.isWorker()) {
      idl_array.add_objects({ WorkerNavigator: ['navigator'] });
    } else {
      idl_array.add_objects({ Navigator: ['navigator'] });
    }
  }
);
