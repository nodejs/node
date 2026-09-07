var activatePromiseResolve;

addEventListener('activate', function(evt) {
  evt.waitUntil(new Promise(function(resolve) {
    activatePromiseResolve = resolve;
  }));
});

addEventListener('message', async function(evt) {
  switch (evt.data) {
    case 'CLAIM':
      evt.waitUntil(new Promise(async resolve => {
        await clients.claim();
        evt.source.postMessage('CLAIMED');
        resolve();
      }));
      break;
    case 'ACTIVATE':
      if (typeof activatePromiseResolve !== 'function') {
        throw new Error('Not activating!');
      }
      activatePromiseResolve();
      break;
    default:
      throw new Error('Unknown message!');
  }
});

addEventListener('fetch', function(evt) {
  evt.respondWith(new Response('Hello world'));
});
