importScripts('./dispatcher.js');

const params = new URLSearchParams(location.search);
const uuid = params.get('uuid');

// The fetch handler must be registered before parsing the main script response.
// So do it here, for future use.
fetchHandler = () => {}
addEventListener('fetch', e => {
  fetchHandler(e);
});

// Force ServiceWorker to immediately activate itself.
addEventListener('install', event => {
  skipWaiting();
});

let executeOrders = async function() {
  while(true) {
    let task = await receive(uuid);
    eval(`(async () => {${task}})()`);
  }
};
executeOrders();
