// META: global=window
// META: title=realm of Response bytes()

"use strict";

promise_test(async () => {
  await new Promise(resolve => {
    onload = resolve;
  });

  let iframe = document.createElement("iframe");
  document.body.appendChild(iframe);
  iframe.srcdoc = "<!doctype html>";
  await new Promise(resolve => {
    iframe.onload = resolve;
  });

  let otherRealm = iframe.contentWindow;

  let ab = await window.Response.prototype.bytes.call(new otherRealm.Response(""));

  assert_true(ab instanceof otherRealm.Uint8Array, "Uint8Array should be created in receiver's realm");
  assert_false(ab instanceof Uint8Array, "Uint8Array should not be created in the bytes() methods's realm");
}, "realm of the Uint8Array from Response bytes()");
