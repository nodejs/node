// testharness file with ShadowRealm utilities to be imported in the realm
// hosting the ShadowRealm

/**
 * Convenience function for evaluating some async code in the ShadowRealm and
 * waiting for the result.
 *
 * In case of error, this function intentionally exposes the stack trace (if it
 * is available) to the hosting realm, for debugging purposes.
 *
 * @param {ShadowRealm} realm - the ShadowRealm to evaluate the code in
 * @param {string} asyncBody - the code to evaluate; will be put in the body of
 *   an async function, and must return a value explicitly if a value is to be
 *   returned to the hosting realm.
 */
globalThis.shadowRealmEvalAsync = function (realm, asyncBody) {
  return new Promise(realm.evaluate(`
    (resolve, reject) => {
      (async () => {
        ${asyncBody}
      })().then(resolve, (e) => reject(e.toString() + "\\n" + (e.stack || "")));
    }
  `));
};

/**
 * Convenience adaptor function for fetch() that can be passed to
 * setShadowRealmGlobalProperties() (see testharness-shadowrealm-inner.js).
 * Used to adapt the hosting realm's fetch(), if present, to fetch a resource
 * and pass its text through the callable boundary to the ShadowRealm.
 */
globalThis.fetchAdaptor = (resource) => (resolve, reject) => {
  fetch(resource)
    .then(res => res.text())
    .then(resolve, (e) => reject(e.toString()));
};

let workerMessagePortPromise;
/**
 * Used when the hosting realm is a worker. This value is a Promise that
 * resolves to a function that posts a message to the worker's message port,
 * just like postMessage(). The message port is only available asynchronously in
 * SharedWorkers and ServiceWorkers.
 */
globalThis.getPostMessageFunc = async function () {
  if (typeof postMessage === "function") {
    return postMessage;  // postMessage available directly in dedicated worker
  }

  if (workerMessagePortPromise) {
    return await workerMessagePortPromise;
  }

  throw new Error("getPostMessageFunc is intended for Worker scopes");
}

// Port available asynchronously in shared worker, but not via an async func
let savedResolver;
if (globalThis.constructor.name === "SharedWorkerGlobalScope") {
  workerMessagePortPromise = new Promise((resolve) => {
    savedResolver = resolve;
  });
  addEventListener("connect", function (event) {
    const port = event.ports[0];
    savedResolver(port.postMessage.bind(port));
  });
} else if (globalThis.constructor.name === "ServiceWorkerGlobalScope") {
  workerMessagePortPromise = new Promise((resolve) => {
    savedResolver = resolve;
  });
  addEventListener("message", (e) => {
    if (typeof e.data === "object" && e.data !== null && e.data.type === "connect") {
      const client = e.source;
      savedResolver(client.postMessage.bind(client));
    }
  });
}

/**
 * Used when the hosting realm does not permit dynamic import, e.g. in
 * ServiceWorkers or AudioWorklets. Requires an adaptor function such as
 * fetchAdaptor() above, or an equivalent if fetch() is not present in the
 * hosting realm.
 *
 * @param {ShadowRealm} realm - the ShadowRealm in which to setup a
 *   fakeDynamicImport() global function.
 * @param {function} adaptor - an adaptor function that does what fetchAdaptor()
 *   does.
 */
globalThis.setupFakeDynamicImportInShadowRealm = function(realm, adaptor) {
  function fetchModuleTextExecutor(url) {
    return (resolve, reject) => {
      new Promise(adaptor(url))
        .then(text => realm.evaluate(text + ";\nundefined"))
        .then(resolve, (e) => reject(e.toString()));
    }
  }

  realm.evaluate(`
    (fetchModuleTextExecutor) => {
      globalThis.fakeDynamicImport = function (url) {
        return new Promise(fetchModuleTextExecutor(url));
      }
    }
  `)(fetchModuleTextExecutor);
};

/**
 * Used when the hosting realm does not expose fetch(), i.e. in worklets. The
 * port on the other side of the channel needs to send messages starting with
 * 'fetchRequest::' and listen for messages starting with 'fetchResult::'. See
 * testharness-shadowrealm-audioworkletprocessor.js.
 *
 * @param {port} MessagePort - the message port on which to listen for fetch
 *   requests
 */
globalThis.setupFakeFetchOverMessagePort = function (port) {
  port.addEventListener("message", (event) => {
    if (typeof event.data !== "string" || !event.data.startsWith("fetchRequest::")) {
      return;
    }

    fetch(event.data.slice("fetchRequest::".length))
      .then(res => res.text())
      .then(
        text => port.postMessage(`fetchResult::success::${text}`),
        error => port.postMessage(`fetchResult::fail::${error}`),
      );
  });
  port.start();
}

/**
 * Returns a message suitable for posting with postMessage() that will signal to
 * the test harness that the tests are finished and there was an error in the
 * setup code.
 *
 * @param {message} any - error
 */
globalThis.createSetupErrorResult = function (message) {
  return {
    type: "complete",
    tests: [],
    asserts: [],
    status: {
      status: 1, // TestsStatus.ERROR,
      message: String(message),
      stack: typeof message === "object" && message !== null && "stack" in message ? message.stack : undefined,
    },
  };
};
