// Based on similar tests in html/infrastructure/safe-passing-of-structured-data/shared-array-buffers/.
//
// This file is simplified from the one there, because it only tests that the
// module can be passed and that functions can be run. The SharedArrayBuffer
// version also tests that the memory is shared between the agents.

"use strict";

function createWasmModule() {
  return fetch('incrementer.wasm')
      .then(response => {
        if (!response.ok)
          throw new Error(response.statusText);
        return response.arrayBuffer();
      })
      .then(WebAssembly.compile);
}

function testModule(module) {
  let instance = new WebAssembly.Instance(module);
  let increment = instance.exports["increment"];
  assert_equals(typeof increment, "function", `The type of the increment export should be "function", got ${typeof increment}`);
  let result = increment(42);
  assert_equals(result, 43, `increment(42) should be 43, got ${result}`);
}

self.testSharingViaIncrementerScript = (t, whereToListen, whereToListenLabel, whereToSend, whereToSendLabel, origin) => {
  return createWasmModule().then(module => {
    return new Promise(resolve => {

      whereToListen.onmessage = t.step_func(({ data }) => {
        switch (data.message) {
          case "module received": {
            testModule(data.module);
            resolve();
            break;
          }
        }
      });

      whereToSend.postMessage({ message: "send module", module }, origin);
    });
  });
};

self.setupDestinationIncrementer = (whereToListen, whereToSendBackTo, origin) => {
  whereToListen.onmessage = ({ data }) => {
    switch (data.message) {
      case "send module": {
        let module = data.module;
        testModule(data.module);
        whereToSendBackTo.postMessage({ message: "module received", module }, origin);
        break;
      }
    }
  };
};
