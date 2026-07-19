// META: title=Blob methods return new objects ([NewObject])
// META: global=window,worker
'use strict';

['stream', 'text', 'arrayBuffer', 'bytes'].forEach(method => {
  test(() => {
    const blob = new Blob(['PASS']);
    const a = blob[method]();
    const b = blob[method]();
    assert_not_equals(a, b, `Blob.${method}() must return a new object`);
  }, `Blob.${method}() returns [NewObject]`);
});
