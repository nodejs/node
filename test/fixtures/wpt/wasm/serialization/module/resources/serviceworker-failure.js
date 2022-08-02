// Based on similar tests in html/infrastructure/safe-passing-of-structured-data/shared-array-buffers/.
"use strict";
self.importScripts("/resources/testharness.js");
self.importScripts("./create-empty-wasm-module.js");

let state = "start in worker";

self.onmessage = e => {
  if (e.data === "start in window") {
    assert_equals(state, "start in worker");
    e.source.postMessage(state);
    state = "we are expecting a messageerror due to the window sending us a WebAssembly.Module";
  } else if (e.data === "we are expecting a messageerror due to the worker sending us a WebAssembly.Module") {
    assert_equals(state, "onmessageerror was received in worker");
    e.source.postMessage(createEmptyWasmModule());
    state = "done in worker";
  } else {
    e.source.postMessage(`worker onmessage was reached when in state "${state}" and data ${e.data}`);
  }
};

self.onmessageerror = e => {
  if (state === "we are expecting a messageerror due to the window sending us a WebAssembly.Module") {
    assert_equals(e.constructor.name, "ExtendableMessageEvent", "type");
    assert_equals(e.data, null, "data");
    assert_equals(e.origin, self.origin, "origin");
    assert_not_equals(e.source, null, "source");
    assert_equals(e.ports.length, 0, "ports length");

    state = "onmessageerror was received in worker";
    e.source.postMessage(state);
  } else {
    e.source.postMessage(`worker onmessageerror was reached when in state "${state}" and data ${e.data}`);
  }
};
