// META: global=window,worker,shadowrealm
// META: script=/common/gc.js
'use strict';

promise_test(async () => {

  let written = false;
  const promise = (() => {
    const rs = new WritableStream({
      write() {
        written = true;
      }
    });
    const writer = rs.getWriter();
    return writer.write('something');
  })();
  await garbageCollect();
  await promise;
  assert_true(written);

}, 'A WritableStream and its writer should not be garbage collected while there is a write promise pending');
