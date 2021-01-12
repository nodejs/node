// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

'use strict';

// https://wicg.github.io/cors-rfc1918/

idl_test(
  ['cors-rfc1918'],
  ['html', 'dom'],
  idlArray => {
    if (self.GLOBAL.isWorker()) {
      idlArray.add_objects({
        WorkerGlobalScope: ['self'],
      });
    } else {
      idlArray.add_objects({
        Document: ['document'],
      });
    }
  }
);
