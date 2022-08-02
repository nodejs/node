importScripts("./test-incrementer.js");
importScripts("./create-empty-wasm-module.js");

let state = "send-sw-failure"
onconnect = initialE => {
  let port = initialE.source;
  port.postMessage(state)
  port.onmessage = e => {
    if(state === "" && e.data === "send-window-failure") {
      port.postMessage(createEmptyWasmModule())
    } else {
      port.postMessage("failure")
    }
  }
  port.onmessageerror = e => {
    if(state === "send-sw-failure") {
      port.postMessage("send-sw-failure-success")
      state = ""
    }
  }
}
