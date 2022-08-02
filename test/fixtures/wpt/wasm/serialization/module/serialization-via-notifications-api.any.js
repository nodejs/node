"use strict";

function createEmptyWasmModule() {
  return new WebAssembly.Module(
      new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
}

test(() => {
  assert_throws_dom("DataCloneError", () => {
    new Notification("Bob: Hi", { data: createEmptyWasmModule() });
  })
}, "WebAssembly.Module cloning via the Notifications API's data member: basic case");

test(() => {
  let getter1Called = false;
  let getter2Called = false;

  assert_throws_dom("DataCloneError", () => {
    new Notification("Bob: Hi", { data: [
      { get x() { getter1Called = true; return 5; } },
      createEmptyWasmModule(),
      { get x() { getter2Called = true; return 5; } }
    ]});
  });

  assert_true(getter1Called, "The getter before the SAB must have been called");
  assert_false(getter2Called, "The getter after the SAB must not have been called");
}, "WebAssembly.Module cloning via the Notifications API's data member: is interleaved correctly");
