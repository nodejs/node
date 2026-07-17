// META: title=Blob Text
// META: script=../support/Blob.js
'use strict';

promise_test(async () => {
  const blob = new Blob(["PASS"]);
  const text = await blob.text();
  assert_equals(text, "PASS");
}, "Blob.text()")

promise_test(async () => {
  const blob = new Blob();
  const text = await blob.text();
  assert_equals(text, "");
}, "Blob.text() empty blob data")

promise_test(async () => {
  const blob = new Blob(["P", "A", "SS"]);
  const text = await blob.text();
  assert_equals(text, "PASS");
}, "Blob.text() multi-element array in constructor")

promise_test(async () => {
  const non_unicode = "\u0061\u030A";
  const input_arr = new TextEncoder().encode(non_unicode);
  const blob = new Blob([input_arr]);
  const text = await blob.text();
  assert_equals(text, non_unicode);
}, "Blob.text() non-unicode")

promise_test(async () => {
  const blob = new Blob(["PASS"], { type: "text/plain;charset=utf-16le" });
  const text = await blob.text();
  assert_equals(text, "PASS");
}, "Blob.text() different charset param in type option")

promise_test(async () => {
  const non_unicode = "\u0061\u030A";
  const input_arr = new TextEncoder().encode(non_unicode);
  const blob = new Blob([input_arr], { type: "text/plain;charset=utf-16le" });
  const text = await blob.text();
  assert_equals(text, non_unicode);
}, "Blob.text() different charset param with non-ascii input")

promise_test(async () => {
  const input_arr = new Uint8Array([192, 193, 245, 246, 247, 248, 249, 250, 251,
      252, 253, 254, 255]);
  const blob = new Blob([input_arr]);
  const text = await blob.text();
  assert_equals(text, "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd" +
      "\ufffd\ufffd\ufffd\ufffd");
}, "Blob.text() invalid utf-8 input")

promise_test(async () => {
  const input_arr = new Uint8Array([192, 193, 245, 246, 247, 248, 249, 250, 251,
      252, 253, 254, 255]);
  const blob = new Blob([input_arr]);
  const text_results = await Promise.all([blob.text(), blob.text(),
      blob.text()]);
  for (let text of text_results) {
    assert_equals(text, "\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd" +
        "\ufffd\ufffd\ufffd\ufffd");
  }
}, "Blob.text() concurrent reads")
