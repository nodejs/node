/**
 * AudioWorkletProcessor intended for hosting a ShadowRealm and running a test
 * inside of that ShadowRealm.
 */
globalThis.TestRunner = class TestRunner extends AudioWorkletProcessor {
  constructor() {
    super();
    this.createShadowRealmAndStartTests();
  }

  /**
   * Fetch adaptor function intended as a drop-in replacement for fetchAdaptor()
   * (see testharness-shadowrealm-outer.js), but it does not assume fetch() is
   * present in the realm. Instead, it relies on setupFakeFetchOverMessagePort()
   * having been called on the port on the other side of this.port's channel.
   */
  fetchOverPortExecutor(resource) {
    return (resolve, reject) => {
      const listener = (event) => {
        if (typeof event.data !== "string" || !event.data.startsWith("fetchResult::")) {
          return;
        }

        const result = event.data.slice("fetchResult::".length);
        if (result.startsWith("success::")) {
          resolve(result.slice("success::".length));
        } else {
          reject(result.slice("fail::".length));
        }

        this.port.removeEventListener("message", listener);
      }
      this.port.addEventListener("message", listener);
      this.port.start();
      this.port.postMessage(`fetchRequest::${resource}`);
    }
  }

  /**
   * Async method, which is patched over in
   * (test).any.audioworklet-shadowrealm.js; see serve.py
   */
  async createShadowRealmAndStartTests() {
    throw new Error("Forgot to overwrite this method!");
  }

  /** Overrides AudioWorkletProcessor.prototype.process() */
  process() {
    return false;
  }
};
registerProcessor("test-runner", TestRunner);
