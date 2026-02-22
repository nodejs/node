// META: title=realm of Response arrayBuffer()

'use strict';

promise_test(async () => {
  await new Promise(resolve => {
    onload = resolve;
  });

  let iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  iframe.srcdoc = '<!doctype html>';
  await new Promise(resolve => {
    iframe.onload = resolve;
  });

  let otherRealm = iframe.contentWindow;

  let ab = await window.Response.prototype.arrayBuffer.call(new otherRealm.Response(''));

  assert_true(ab instanceof otherRealm.ArrayBuffer, "ArrayBuffer should be created in receiver's realm");
  assert_false(ab instanceof ArrayBuffer, "ArrayBuffer should not be created in the arrayBuffer() methods's realm");
}, 'realm of the ArrayBuffer from Response arrayBuffer()');
