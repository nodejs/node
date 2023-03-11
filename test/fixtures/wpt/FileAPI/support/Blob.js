'use strict'

self.test_blob = (fn, expectations) => {
  var expected = expectations.expected,
      type = expectations.type,
      desc = expectations.desc;

  var t = async_test(desc);
  t.step(function() {
    var blob = fn();
    assert_true(blob instanceof Blob);
    assert_false(blob instanceof File);
    assert_equals(blob.type, type);
    assert_equals(blob.size, expected.length);

    var fr = new FileReader();
    fr.onload = t.step_func_done(function(event) {
      assert_equals(this.result, expected);
    }, fr);
    fr.onerror = t.step_func(function(e) {
      assert_unreached("got error event on FileReader");
    });
    fr.readAsText(blob, "UTF-8");
  });
}

self.test_blob_binary = (fn, expectations) => {
  var expected = expectations.expected,
      type = expectations.type,
      desc = expectations.desc;

  var t = async_test(desc);
  t.step(function() {
    var blob = fn();
    assert_true(blob instanceof Blob);
    assert_false(blob instanceof File);
    assert_equals(blob.type, type);
    assert_equals(blob.size, expected.length);

    var fr = new FileReader();
    fr.onload = t.step_func_done(function(event) {
      assert_true(this.result instanceof ArrayBuffer,
                  "Result should be an ArrayBuffer");
      assert_array_equals(new Uint8Array(this.result), expected);
    }, fr);
    fr.onerror = t.step_func(function(e) {
      assert_unreached("got error event on FileReader");
    });
    fr.readAsArrayBuffer(blob);
  });
}

// Assert that two TypedArray objects have the same byte values
self.assert_equals_typed_array = (array1, array2) => {
  const [view1, view2] = [array1, array2].map((array) => {
    assert_true(array.buffer instanceof ArrayBuffer,
      'Expect input ArrayBuffers to contain field `buffer`');
    return new DataView(array.buffer, array.byteOffset, array.byteLength);
  });

  assert_equals(view1.byteLength, view2.byteLength,
    'Expect both arrays to be of the same byte length');

  const byteLength = view1.byteLength;

  for (let i = 0; i < byteLength; ++i) {
    assert_equals(view1.getUint8(i), view2.getUint8(i),
      `Expect byte at buffer position ${i} to be equal`);
  }
}
