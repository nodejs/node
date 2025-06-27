// META: title=Blob slice overflow
'use strict';

var text = '';

for (var i = 0; i < 2000; ++i) {
  text += 'A';
}

test(function() {
  var blob = new Blob([text]);
  var sliceBlob = blob.slice(-1, blob.size);
  assert_equals(sliceBlob.size, 1, "Blob slice size");
}, "slice start is negative, relativeStart will be max((size + start), 0)");

test(function() {
  var blob = new Blob([text]);
  var sliceBlob = blob.slice(blob.size + 1, blob.size);
  assert_equals(sliceBlob.size, 0, "Blob slice size");
}, "slice start is greater than blob size, relativeStart will be min(start, size)");

test(function() {
  var blob = new Blob([text]);
  var sliceBlob = blob.slice(blob.size - 2, -1);
  assert_equals(sliceBlob.size, 1, "Blob slice size");
}, "slice end is negative, relativeEnd will be max((size + end), 0)");

test(function() {
  var blob = new Blob([text]);
  var sliceBlob = blob.slice(blob.size - 2, blob.size + 999);
  assert_equals(sliceBlob.size, 2, "Blob slice size");
}, "slice end is greater than blob size, relativeEnd will be min(end, size)");
