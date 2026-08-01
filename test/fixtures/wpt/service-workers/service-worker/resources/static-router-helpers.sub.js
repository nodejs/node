// Helper functions for ServiceWorker static routing API.
//
// test-helpers.sub.js must be loaded before using this.

// Get a dictionary of information recorded inside ServiceWorker.
// It includes:
// - request URL and mode.
// - errors.
//
// See: static-router-sw.js for details.
const get_info_from_worker =
    async worker => {
  const promise = new Promise(function(resolve) {
      var channel = new MessageChannel();
      channel.port1.onmessage = function(msg) { resolve(msg); };
      worker.postMessage({port: channel.port2}, [channel.port2]);
    });
  const message = await promise;

  return message.data;
}

// Reset information stored inside ServiceWorker.
const reset_info_in_worker =
    async worker => {
  const promise = new Promise(function(resolve) {
      var channel = new MessageChannel();
      channel.port1.onmessage = function(msg) { resolve(msg); };
      worker.postMessage({port: channel.port2, reset: true}, [channel.port2]);
    });
  await promise;
}

// This script's directory name. It is used for specifying test files.
const scriptDir = document.currentScript.src.match(/.*\//)[0];

// Register the ServiceWorker and wait until activated.
// {ruleKey} represents the key of routerRules defined in router-rules.js.
// {swScript} represents the service worker source URL.
// {swScope} represents the service worker resource scope.
const registerAndActivate = async (test, ruleKey, swScript, swScope) => {
  if (!swScript) {
    swScript = scriptDir + 'static-router-sw.js'
  }
  if (!swScope) {
    swScope = scriptDir;
  }
  const swURL = `${swScript}?key=${ruleKey}`;
  const reg = await service_worker_unregister_and_register(
    test, swURL, swScope, { type: 'module' });
  add_completion_callback(() => reg.unregister());
  const worker = reg.installing;
  await wait_for_state(test, worker, 'activated');

  return worker;
};

// Create iframe with the given url. This automatically removes the iframe in a
// cleanup.
const createIframe = async (t, url) => {
  const iframe = await with_iframe(url);
  t.add_cleanup(() => iframe.remove());

  return iframe;
};

// Register a service worker, then create an iframe at url.
function iframeTest(url, ruleKey, callback, name) {
  return promise_test(async t => {
    const worker = await registerAndActivate(t, ruleKey);
    const iframe = await createIframe(t, url);
    await callback(t, iframe.contentWindow, worker);
  }, name);
};

function randomString() {
  let result = "";
  for (let i = 0; i < 5; i++) {
    result += String.fromCharCode(97 + Math.floor(Math.random() * 26));
  }
  return result;
}
