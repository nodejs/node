// META: global=window,worker,shadowrealm-in-window
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

'use strict';

// https://w3c.github.io/hr-time/

idl_test(
  ['hr-time'],
  ['html', 'dom'],
  async idl_array => {
    if (self.GLOBAL.isShadowRealm()) {
      return;
    }

    if (self.GLOBAL.isWorker()) {
      idl_array.add_objects({ WorkerGlobalScope: ['self'] });
    } else {
      idl_array.add_objects({ Window: ['self'] });
    }
    idl_array.add_objects({
      Performance: ['performance'],
    });
  }
);
