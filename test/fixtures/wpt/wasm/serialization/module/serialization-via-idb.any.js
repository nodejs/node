// META: script=/IndexedDB/resources/support.js
"use strict";

function createEmptyWasmModule() {
  return new WebAssembly.Module(
      new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
}

async_test(t => {
  const openReq = createdb(t);

  openReq.onupgradeneeded = e => {
    const db = e.target.result;
    const store = db.createObjectStore("store", { keyPath: "key" });

    assert_throws_dom("DataCloneError", () => {
      store.put({ key: 1, property: createEmptyWasmModule() });
    });
    t.done();
  };
}, "WebAssembly.Module cloning via IndexedDB: basic case");

async_test(t => {
  const openReq = createdb(t);

  openReq.onupgradeneeded = e => {
    const db = e.target.result;
    const store = db.createObjectStore("store", { keyPath: "key" });

    let getter1Called = false;
    let getter2Called = false;

    assert_throws_dom("DataCloneError", () => {
      store.put({ key: 1, property: [
        { get x() { getter1Called = true; return 5; } },
        createEmptyWasmModule(),
        { get x() { getter2Called = true; return 5; } }
      ]});
    });

    assert_true(getter1Called, "The getter before the WebAssembly.Module must have been called");
    assert_false(getter2Called, "The getter after the WebAssembly.Module must not have been called");
    t.done();
  };
}, "WebAssembly.Module cloning via the IndexedDB: is interleaved correctly");
