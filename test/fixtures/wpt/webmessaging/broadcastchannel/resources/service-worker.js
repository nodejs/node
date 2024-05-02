let promise_func = null;
let promise = new Promise(resolve => promise_func = resolve);

const SERVICE_WORKER_TEST_CHANNEL_NAME = 'service worker';
const bc3 = new BroadcastChannel(SERVICE_WORKER_TEST_CHANNEL_NAME);
bc3.onmessage = e => {
  bc3.postMessage('done');
  promise_func();
};
bc3.postMessage('from worker');

// Ensure that the worker stays alive for the duration of the test
self.addEventListener('install', evt => {
  evt.waitUntil(promise);
});
